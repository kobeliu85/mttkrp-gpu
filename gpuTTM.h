#ifndef _TTMGPU_H

#include <cuda_runtime.h>

#include <helper_cuda.h>
#include <device_launch_parameters.h>

#include <device_functions.h>
#include "TTM.h"
#include "convert.h"
#include "flag.h"

template <typename T>
struct val 
{
	T value;
	unsigned short flag;
};

template <typename T>
struct tensor_gpu
{
	// int *d_i;
	// int *d_j;
	int *d_k;
	T *d_val;
	int d_nnz;  //length
	int d_threadlen; // the length of data single cuda thread 	
	int d_iterlen;  // length/d_threadlen
	tensor_gpu(stensor H_tensor, int thread_len);
	~tensor_gpu();
};

template <typename T, typename type_thread>
struct semitensor_gpu
{	
	int d_nfibs;
	int d_nCols;
	type_thread *d_bflags;
	int *d_first; // each thread first result; index for recahce
	T *d_val;
	T *d_last_partial;
	T *d_recache;
	T *d_blockSum;
	int BLOCK_SIZE;

    unsigned short *d_startflag;
     // unsigned short *d_startflag_backup;
    unsigned short *d_blockflag;
	semitensor_gpu(semiTensor<T> H_tensor, flag<type_thread> h_flag, int BLOCK_SIZE);
	~semitensor_gpu();
};

//difference to see the tempalte 

template <typename T>
tensor_gpu<T>:: tensor_gpu(stensor H_tensor, int thread_len)
{
	d_nnz=((H_tensor.nnz-1)/thread_len+1)*thread_len;
	d_threadlen=thread_len;
	d_iterlen=d_nnz/d_threadlen;
	// int h_nnz=H_tensor.nnz;

	// cudaMalloc((void **)&d_i,sizeof(int)*d_nnz);
	// cudaMalloc((void **)&d_j,sizeof(int)*d_nnz);
	cudaMalloc((void **)&d_k,sizeof(int)*d_nnz);
	cudaMalloc((void **)&d_val,sizeof(T)*d_nnz);

	// cudaMemset(d_i,0,sizeof(int)*d_nnz);
	// cudaMemset(d_j,0,sizeof(int)*d_nnz);
	cudaMemset(d_k,0,sizeof(int)*d_nnz);
	cudaMemset(d_val,0,sizeof(T)*d_nnz);
	
	// cudaMemcpy(d_i,H_tensor.i,sizeof(int)*d_nnz,cudaMemcpyHostToDevice);
	// cudaMemcpy(d_j,H_tensor.j,sizeof(int)*d_nnz,cudaMemcpyHostToDevice);
	cudaMemcpy(d_k,H_tensor.k,sizeof(int)*d_nnz,cudaMemcpyHostToDevice);
	cudaMemcpy(d_val,H_tensor.val,sizeof(T)*d_nnz,cudaMemcpyHostToDevice);	
}

template <typename T>
tensor_gpu<T>::~tensor_gpu()
{
	// cudaFree(d_i);
	// cudaFree(d_j);
	cudaFree(d_k);
	cudaFree(d_val);
}

template <typename T, typename type_thread>
semitensor_gpu<T,type_thread>::semitensor_gpu(semiTensor<T> H_tensor, 
											  flag<type_thread> h_flag, 
											  int BLOCK_SIZE)
{
	int d_nfibs=H_tensor.nfibs;
	int d_nCols=H_tensor.R;
    
    int nnz=H_tensor.nnz;
    int bit_len=sizeof(type_thread)*8;
    int d_nnz=((H_tensor.nnz-1)/bit_len+1);

    int nBlock=(d_nnz-1)/BLOCK_SIZE+1;

	cudaMalloc((void **)&d_val,sizeof(T)*d_nfibs*d_nCols);
	cudaMemset(d_val,0,sizeof(T)*d_nfibs*d_nCols);

	cudaMalloc((void **)&d_bflags,sizeof(type_thread)*d_nnz);
	cudaMemcpy(d_bflags,h_flag.bit_flag,sizeof(type_thread)*d_nnz,cudaMemcpyHostToDevice);

	
	cudaMalloc((void **)&d_first,sizeof(int)*d_nnz);
	cudaMemcpy(d_first,h_flag.first,sizeof(int)*d_nnz,cudaMemcpyHostToDevice);

	cudaMalloc((void **)&d_last_partial,sizeof(T)*d_nnz*d_nCols);
	cudaMemset((void **)&d_last_partial,0,sizeof(T)*d_nnz*d_nCols);

	cudaMalloc((void **)&d_recache,sizeof(T)*d_nfibs*d_nCols);
	cudaMemset((void **)&d_recache,0,sizeof(T)*d_nfibs*d_nCols);

	cudaMalloc((void **)&d_startflag,sizeof(unsigned short)*d_nnz);
	cudaMemcpy(d_startflag,h_flag.startflag,sizeof(unsigned short)*d_nnz,cudaMemcpyHostToDevice);

	// cudaMalloc((void **)&d_startflag_backup,sizeof(unsigned short)*d_nnz);
	// cudaMemcpy(d_startflag_backup,h_flag.startflag,sizeof(unsigned short)*d_nnz,cudaMemcpyHostToDevice);

	cudaMalloc((void **)&d_blockSum,sizeof(T)*nBlock*d_nCols);
	cudaMemset(d_blockSum,-1,sizeof(T)*nBlock*d_nCols);

	cudaMalloc((void **)&d_blockflag,sizeof(unsigned short)*nBlock);
	cudaMemcpy(d_blockflag,h_flag.blockflag,sizeof(unsigned short)*nBlock,cudaMemcpyHostToDevice);
}

template <typename T, typename type_thread>
semitensor_gpu<T,type_thread>::~semitensor_gpu()
{
	cudaFree(d_val);
	cudaFree(d_bflags);
	cudaFree(d_first);
	cudaFree(d_last_partial);
	cudaFree(d_recache);
	cudaFree(d_startflag);
	cudaFree(d_blockSum);
	cudaFree(d_blockflag);
}

template <typename T>
void transpose_tensor(soa_tensor<T> &H_tensor, int threadLen)
{
	int nnz=H_tensor.nnz;
	int flagLen=(nnz-1)/threadLen+1;

	// int *H_i=(int *)malloc(sizeof(int)*flagLen*threadLen);
	// int *H_j=(int *)malloc(sizeof(int)*flagLen*threadLen);
	int *H_k=(int *)malloc(sizeof(int)*flagLen*threadLen);
	T *H_val=(T *)malloc(sizeof(T)*flagLen*threadLen);

	// memset(H_i,0,sizeof(int)*flagLen*threadLen);
	// memset(H_j,0,sizeof(int)*flagLen*threadLen);
	memset(H_k,0,sizeof(int)*flagLen*threadLen);
	memset(H_val,0,sizeof(T)*flagLen*threadLen);

	for(int i=0;i<nnz;i++)
	{
		int row=i/threadLen;
		int col=i%threadLen;

		int index=col*flagLen+row;
		// H_i[index]=H_tensor.i[i];
		// H_j[index]=H_tensor.j[i];
		H_k[index]=H_tensor.k[i];
		H_val[index]=H_tensor.val[i];
	}

	// free(H_tensor.i);
	// free(H_tensor.j);
	free(H_tensor.k);
	free(H_tensor.val);

	// H_tensor.i=H_i;
	// H_tensor.j=H_j;
	H_tensor.k=H_k;
	H_tensor.val=H_val;
}

template <typename T>
void transpose_matrix(T *val, int nRows, int nCols)
{
 	T *tmp=(T *)malloc(sizeof(T)*nRows*nCols);
 	for(int i=0;i<nRows;i++)
 		for(int j=0;j<nCols;j++)
 		{
 			tmp[j*nRows+i]=val[i*nCols+j];
 		}

 	free(val);
 	val=tmp;
}

template <typename T, typename type_thread>
__global__ void TTMgpu(tensor_gpu<T> D_ltensor, T *D_matrix, int nRows, int nCols, semitensor_gpu<T,type_thread> D_rtensor, unsigned int threadlen);

template <typename T>
__global__ void segmentedscan(T* last_partial, T *blockSum, unsigned short *blockflag, unsigned short *startflag, int d_nnz);

template <typename T, typename type_thread>
__global__ void mergeSum(T *last_partial, T * recache, int *first, type_thread *bflags,int d_nnz);

template <typename T, typename type_thread>
void callTTM(stensor ltensor, T *matrix, int nRows, int nCols, semitensor rtensor, type_thread threadtype)
{
	flag<T> tflag = flag<T>(rtensor);

	int threadlen = sizeof(threadtype)*8;
	transpose_matrix(matrix,nRows,nCols);
	T *D_matrix;

	cudaMalloc(D_matrix,sizeof(T)*nRows*nCols);
	cudaMemcpy(D_matrix,sizeof(T)*nRows*nCols,cudaMemcpyHostToDevice);

	transpose_tensor(ltensor,threadlen);

	int BLOCK_SIZE=1024;
	
	tensor_gpu<T> D_ltensor=tensor_gpu<T>(ltensor,threadlen);
	semitensor_gpu<T,type_thread> D_rtensor=semitensor_gpu<T,type_thread>(rtensor,tflag,BLOCK_SIZE);

	int nnz=D_rtensor.d_nnz;
	int rank=D_rtensor.nCols;

	nnz = 32-nnz%32+32;
	int Gridsize = (nnz -1)/BLOCK_SIZE +1;
    

    
	TTMgpu<<<Gridsize,BLOCK_SIZE>>>(D_ltensor,
									D_matrix, nRows,nCols, 
									D_rtensor, 
									threadlen);

	
	segmentedscan<<<Gridsize,dim3(BLOCK_SIZE,rank)>>>(D_rtensor.d_last_partial,
														D_rtensor.d_blockSum,
														D_rtensor.d_blockflag,
														D_rtensor.d_startflag,
														D_rtensor.d_nnz);

	mergeSum<<<Gridsize,dim3(BLOCK_SIZE,rank)>>>(D_rtensor.d_last_partial,
												 D_rtensor.d_recache, 
												 D_rtensor.d_first,
												 D_rtensor.blockflag,
												 D_rtensor.d_nnz);


}

 
#endif