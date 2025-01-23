/*
 * Copyright (c) Krzesimir Hyżyk - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Created on 18/11/2024 by Krzesimir Hyżyk
 */



#ifndef _SOLVER_SOLVER_H_
#define _SOLVER_SOLVER_H_ 1



typedef void (*solver_range_scheduler_progress_callback_t)(unsigned int);



typedef void (*solver_callback_t)(double,const double*,double*);



typedef struct _SOLVER_RANGE_SCHEDULER_ENTRY{
	double base;
	double delta;
	unsigned int divisions;
	unsigned int offset;
} solver_range_scheduler_entry_t;



typedef struct _SOLVER_RANGE_SCHEDULER{
	solver_range_scheduler_progress_callback_t callback;
	unsigned int point_size;
	unsigned int max_batch_size;
	unsigned long long int progress_total;
	unsigned long long int progress_offset;
	solver_range_scheduler_entry_t* entries;
	unsigned int last_progress;
} solver_range_scheduler_t;



typedef struct _SOLVER_MEMORY_REGION{
	struct _SOLVER_MEMORY_REGION* next;
	void* address;
	unsigned int space;
} solver_memory_region_t;



typedef struct _SOLVER_CACHE{
	void** data;
	unsigned int length;
	unsigned int capacity;
} solver_cache_t;



typedef struct _SOLVER{
	solver_callback_t callback;
	solver_cache_t* cache;
	unsigned long long int point_count;
	unsigned int point_size;
	unsigned int memory_region_space;
	double time;
	double time_step;
	double min_time_step;
	solver_memory_region_t* data_head;
	solver_memory_region_t* data_tail;
	void* scratch_area;
	void* data;
} solver_t;



void solver_range_scheduler_init(unsigned int point_size,unsigned int max_batch_size,const double* from,const double* to,const unsigned int* divisions,solver_range_scheduler_progress_callback_t callback,solver_range_scheduler_t* out);



void solver_range_scheduler_deinit(solver_range_scheduler_t* rs);



unsigned int solver_range_scheduler_get(solver_range_scheduler_t* rs,double** out);



void solver_cache_init(unsigned int capacity,solver_cache_t* out);



void solver_cache_deinit(solver_cache_t* cache);



unsigned long long int solver_cache_get_size(solver_cache_t* cache);



void solver_init(solver_callback_t callback,unsigned int point_size,double time_step,double min_time_step,const double* first_point,solver_cache_t* cache,solver_t* out);



void solver_deinit(solver_t* solver);



const double* solver_get_last_point(const solver_t* solver);



void* solver_iterate(const solver_t* solver,void* ptr,const double** points,unsigned int* point_count);



void solver_step_batch_rk4(solver_t* solver,unsigned int count);



void solver_step_batch_rkf45(solver_t* solver,unsigned int count);



void _solver_step_batch_rkf45_internal(solver_t* solver,unsigned int count,void* in,void* out);



#endif
