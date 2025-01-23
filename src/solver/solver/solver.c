/*
 * Copyright (c) Krzesimir Hyżyk - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Created on 18/11/2024 by Krzesimir Hyżyk
 */



#include <immintrin.h>
#include <math.h>
#include <solver/solver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>



#define MEMORY_REGION_SIZE 0x100000



static void* _cache_get_memory(solver_cache_t* cache){
	if (cache->length){
		cache->length--;
		return cache->data[cache->length];
	}
	return mmap(NULL,MEMORY_REGION_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE,-1,0);
}



static void _cache_put_memory(solver_cache_t* cache,void* address){
	if (cache->length>=cache->capacity){
		munmap(address,MEMORY_REGION_SIZE);
		return;
	}
	cache->data[cache->length]=address;
	cache->length++;
}



void solver_range_scheduler_init(unsigned int point_size,unsigned int max_batch_size,const double* from,const double* to,const unsigned int* divisions,solver_range_scheduler_progress_callback_t callback,solver_range_scheduler_t* out){
	out->callback=callback;
	out->point_size=point_size;
	out->max_batch_size=max_batch_size;
	out->progress_total=1;
	out->progress_offset=0;
	out->entries=malloc(point_size*sizeof(solver_range_scheduler_entry_t));
	out->last_progress=(callback?0:100);
	for (unsigned int i=0;i<point_size;i++){
		(out->entries+i)->base=from[i];
		if (divisions[i]>1){
			(out->entries+i)->delta=(to[i]-from[i])/divisions[i];
			(out->entries+i)->divisions=divisions[i];
			out->progress_total*=divisions[i];
		}
		else{
			(out->entries+i)->delta=0.0;
			(out->entries+i)->divisions=1;
		}
		(out->entries+i)->offset=0;
	}
}



void solver_range_scheduler_deinit(solver_range_scheduler_t* rs){
	free(rs->entries);
}



unsigned int solver_range_scheduler_get(solver_range_scheduler_t* rs,double** out){
	unsigned int count=0;
	if (!(*out)){
		*out=malloc(rs->max_batch_size*rs->point_size*sizeof(double));
	}
	for (double* ptr=*out;count<rs->max_batch_size&&rs->progress_offset<rs->progress_total;count++){
		_Bool increase_next_offset=1;
		for (unsigned int i=0;i<rs->point_size;i++){
			ptr[i]=(rs->entries+i)->base+(rs->entries+i)->delta*(rs->entries+i)->offset;
			if (!increase_next_offset){
				continue;
			}
			(rs->entries+i)->offset++;
			if ((rs->entries+i)->offset<(rs->entries+i)->divisions){
				increase_next_offset=0;
				continue;
			}
			(rs->entries+i)->offset=0;
		}
		ptr+=rs->point_size;
		rs->progress_offset++;
		if (increase_next_offset){
			break;
		}
	}
	unsigned int progress=rs->progress_offset*100/rs->progress_total;
	if (progress>rs->last_progress){
		rs->last_progress=progress;
		rs->callback(progress);
	}
	if (!count&&*out){
		free(*out);
		*out=NULL;
	}
	return count;
}



void solver_cache_init(unsigned int capacity,solver_cache_t* out){
	out->data=malloc(capacity*sizeof(void*));
	out->length=0;
	out->capacity=capacity;
}



void solver_cache_deinit(solver_cache_t* cache){
	for (unsigned int i=0;i<cache->length;i++){
		munmap(cache->data[i],MEMORY_REGION_SIZE);
	}
	free(cache->data);
}



unsigned long long int solver_cache_get_size(solver_cache_t* cache){
	return ((unsigned long long int)(cache->length))*MEMORY_REGION_SIZE;
}



void solver_init(solver_callback_t callback,unsigned int point_size,double time_step,double min_time_step,const double* first_point,solver_cache_t* cache,solver_t* out){
	if (point_size&3){
		printf("Unsupported point size\n");
		exit(1);
	}
	out->callback=callback;
	out->cache=cache;
	out->point_count=1;
	out->point_size=point_size*sizeof(double);
	out->memory_region_space=MEMORY_REGION_SIZE/out->point_size;
	out->time_step=time_step;
	out->min_time_step=min_time_step;
	out->time=0.0;
	out->data_head=malloc(sizeof(solver_memory_region_t));
	out->data_head->next=NULL;
	out->data_head->address=_cache_get_memory(cache);
	out->data_head->space=out->memory_region_space-1;
	out->data_tail=out->data_head;
	out->scratch_area=aligned_alloc(32,out->point_size*6);
	out->data=out->data_head->address;
	memcpy(out->data,first_point,out->point_size);
}



void solver_deinit(solver_t* solver){
	for (solver_memory_region_t* region=solver->data_head;region;){
		solver_memory_region_t* next=region->next;
		_cache_put_memory(solver->cache,region->address);
		free(region);
		region=next;
	}
	free(solver->scratch_area);
}



const double* solver_get_last_point(const solver_t* solver){
	return solver->data;
}



void* solver_iterate(const solver_t* solver,void* ptr,const double** points,unsigned int* point_count){
	solver_memory_region_t* region=(ptr?((solver_memory_region_t*)ptr)->next:solver->data_head);
	if (!region){
		return NULL;
	}
	*points=region->address;
	*point_count=solver->memory_region_space-region->space;
	return region;
}



void solver_step_batch_rk4(solver_t* solver,unsigned int count){ // Runge-Kutta [RK4]
	void* in=solver->data;
	void* out=in+solver->point_size;
	void* tmp_in=solver->scratch_area;
	void* tmp_out=solver->scratch_area+solver->point_size;
	if (!solver->data_tail->space){
		solver_memory_region_t* region=malloc(sizeof(solver_memory_region_t));
		region->next=NULL;
		region->address=_cache_get_memory(solver->cache);
		region->space=solver->memory_region_space;
		solver->data_tail->next=region;
		solver->data_tail=region;
		out=region->address;
	}
	if (count>solver->data_tail->space){
		count=solver->data_tail->space;
	}
	solver->point_count+=count;
	solver->data_tail->space-=count;
	double t=solver->time;
	double h2=solver->time_step/2;
	__m256d h2v=_mm256_set1_pd(h2);
	__m256d h3v=_mm256_set1_pd(solver->time_step/3);
	__m256d h6v=_mm256_sub_pd(h2v,h3v);
	for (;count;count--){
		solver->callback(t,in,tmp_out);
		t+=h2;
		for (unsigned int i=0;i<solver->point_size;i+=4*sizeof(double)){
			__m256d a=_mm256_load_pd(tmp_out+i);
			__m256d b=_mm256_load_pd(in+i);
			_mm256_store_pd(out+i,_mm256_fmadd_pd(a,h6v,b));
			_mm256_store_pd(tmp_in+i,_mm256_fmadd_pd(a,h2v,b));
		}
		solver->callback(t,tmp_in,tmp_out);
		for (unsigned int i=0;i<solver->point_size;i+=4*sizeof(double)){
			__m256d a=_mm256_load_pd(tmp_out+i);
			_mm256_store_pd(out+i,_mm256_fmadd_pd(a,h3v,_mm256_load_pd(out+i)));
			_mm256_store_pd(tmp_in+i,_mm256_fmadd_pd(a,h2v,_mm256_load_pd(in+i)));
		}
		solver->callback(t,tmp_in,tmp_out);
		t+=h2;
		for (unsigned int i=0;i<solver->point_size;i+=4*sizeof(double)){
			__m256d a=_mm256_load_pd(tmp_out+i);
			_mm256_store_pd(out+i,_mm256_fmadd_pd(a,h3v,_mm256_load_pd(out+i)));
			_mm256_store_pd(tmp_in+i,_mm256_add_pd(_mm256_load_pd(in+i),a));
		}
		solver->callback(t,tmp_in,tmp_out);
		for (unsigned int i=0;i<solver->point_size;i+=4*sizeof(double)){
			_mm256_store_pd(out+i,_mm256_fmadd_pd(_mm256_load_pd(tmp_out+i),h6v,_mm256_load_pd(out+i)));
		}
		in=out;
		out=in+solver->point_size;
	}
	solver->time=t;
	solver->data=in;
}



void solver_step_batch_rkf45(solver_t* solver,unsigned int count){ // Runge-Kutta-Fehlberg [RK4(5)]
	void* in=solver->data;
	void* out=in+solver->point_size;
	if (!solver->data_tail->space){
		solver_memory_region_t* region=malloc(sizeof(solver_memory_region_t));
		region->next=NULL;
		region->address=_cache_get_memory(solver->cache);
		region->space=solver->memory_region_space;
		solver->data_tail->next=region;
		solver->data_tail=region;
		out=region->address;
	}
	if (count>solver->data_tail->space){
		count=solver->data_tail->space;
	}
	solver->point_count+=count;
	solver->data_tail->space-=count;
	solver->data=out+solver->point_size*(count-1);
	_solver_step_batch_rkf45_internal(solver,count,in,out);
}
