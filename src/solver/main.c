/*
 * Copyright (c) Krzesimir Hyżyk - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Created on 18/11/2024 by Krzesimir Hyżyk
 */



#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <solver/solver.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>



#define PATTERN_UNCONSTRAINED 0
#define PATTERN_NO_LOOP 1
#define PATTERN_NO_STALL 2

#define PATTERN PATTERN_NO_STALL



static solver_range_scheduler_t _range_scheduler;
static pthread_mutex_t _range_scheduler_mutex;
static _Atomic unsigned long long int _total_memory_usage=0;
static _Atomic unsigned long long int _total_cache=0;



static unsigned long long int _get_time(void){
	struct timespec tm;
	clock_gettime(CLOCK_REALTIME,&tm);
	return tm.tv_sec*1000000000ull+tm.tv_nsec;
}



static void _progress_callback(unsigned int progress){
	printf("%u%%\n",progress);
}



static void _callback(double time,const double* in,double* out){
	double s;
	double c;
	sincos(in[3],&s,&c);
	out[0]=in[2]*c;
	out[1]=in[2]*s;
	out[2]=-in[2]*in[2]*0.146130592503022988-s;
	out[3]=in[2]-c/in[2];
}



static void* _solver_thread(void* arg){
	unsigned int index=(unsigned long long int)arg;
	solver_cache_t solver_cache;
	solver_cache_init(1024,&solver_cache);
	solver_t solver;
	double best_x=0.0;
	double best_v=0.0;
	double best_theta=0.0;
	unsigned long long int memory_usage=0;
	double* points=NULL;
	char path[4096];
	snprintf(path,sizeof(path),"build/points/%02u",index);
	int output_fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
	while (1){
		pthread_mutex_lock(&_range_scheduler_mutex);
		unsigned int point_count=solver_range_scheduler_get(&_range_scheduler,&points);
		pthread_mutex_unlock(&_range_scheduler_mutex);
		if (!point_count){
			break;
		}
		for (unsigned int i=0;i<point_count;i++){
			solver_init(_callback,4,0.01,1e-6,points+i*4,&solver_cache,&solver);
			while (1){
				solver_step_batch_rkf45(&solver,8);
				const double* point=solver_get_last_point(&solver);
				if (point[1]<0||point[0]<-16||solver.point_count>=16000000){
					break;
				}
				if (PATTERN==PATTERN_NO_LOOP&&fabsf(point[3])>=1.5707963267948966){
					break;
				}
				if (PATTERN==PATTERN_NO_STALL&&point[3]>=1.0471975511965976){
					break;
				}
			}
			memory_usage+=solver.point_count*solver.point_size;
			if (solver_get_last_point(&solver)[0]<best_x){
				solver_deinit(&solver);
				continue;
			}
			best_x=solver_get_last_point(&solver)[0];
			best_v=points[i*4+2];
			best_theta=points[i*4+3];
			lseek(output_fd,0,SEEK_SET);
			if (ftruncate(output_fd,0)){
				break;
			}
			const double* points=NULL;
			unsigned int point_count=0;
			for (void* ptr=solver_iterate(&solver,NULL,&points,&point_count);ptr;ptr=solver_iterate(&solver,ptr,&points,&point_count)){
				if (write(output_fd,points,point_count*solver.point_size)!=point_count*solver.point_size){
					break;
				}
			}
			solver_deinit(&solver);
		}
	}
	close(output_fd);
	_total_cache+=solver_cache_get_size(&solver_cache);
	solver_cache_deinit(&solver_cache);
	printf("[%02u] x=%6.3lf v=%5.3lf a=%+8.5lf [%llu B]\n",index,best_x,best_v,best_theta,memory_usage);
	_total_memory_usage+=memory_usage;
	return NULL;
}



int main(void){
	double from[4]={0.0,0.00082396828416,0.03840047727997223,9.0/256};
	double to[4]={0.0,0.0,0.03931339428700553,17.0/256};
	unsigned int divisions[4]={0,0,32,1024};
	solver_range_scheduler_init(4,64,from,to,divisions,_progress_callback,&_range_scheduler);
	pthread_mutex_init(&_range_scheduler_mutex,NULL);
	unsigned int thread_count=sysconf(_SC_NPROCESSORS_ONLN);
	pthread_t* threads=malloc(thread_count*sizeof(pthread_t));
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	unsigned long long int start_time=_get_time();
	for (unsigned int i=0;i<thread_count;i++){
		pthread_create(threads+i,&attr,&_solver_thread,(void*)(unsigned long long int)i);
	}
	for (unsigned int i=0;i<thread_count;i++){
		pthread_join(threads[i],NULL);
	}
	unsigned long long int delta_time=_get_time()-start_time;
	printf("%llu n, %llu B, %.3lf s (%llu n/s, %llu B/s) [%llu B cached]\n",_range_scheduler.progress_total,_total_memory_usage,delta_time/1000000/1000.0,_range_scheduler.progress_total*1000000000ull/delta_time,_total_memory_usage*1000000000ull/delta_time,_total_cache);
	free(threads);
	solver_range_scheduler_deinit(&_range_scheduler);
	return 0;
}
