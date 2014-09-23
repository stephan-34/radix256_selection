// radixSort.cpp : Stephan Arens, Sep 2014

#include "stdafx.h"

#include <stdlib.h>
#include <malloc.h>

#define PARTIAL_SORT
// undefine PARTIAL_SORT to get a full sort

#define PERFORMANCE_CPU
// measure performance

#ifdef PERFORMANCE_CPU
#include <Windows.h>
// http://msdn.microsoft.com/de-de/library/windows/desktop/dn553408(v=vs.85).aspx
#endif

typedef unsigned int  uint;
typedef enum HowToSort { keysOnly, keysAndValues } SortKind;

// --------------------------------------------------------
// radix 256 stable sort, i.e. 8 bits per iteration
// Algorithm follows the idea of Merill's 3-step radix sort
// plays ping pong with buffers, i.e. input will be overwritten
// Hence buffer holding the result has to be identified

#define RADIX_BINS (256)
#define RADIX_SHIFT (0x08)

template< SortKind kind >
uint radix256Sort(  	uint* h_inputKeys, 
						uint* h_inputValues, 
						uint* h_outputKeys, 
						uint* h_outputValues, 
						uint numElems )
{
	int toggle = 0; // buffers play ping pong
	const uint radixMask = RADIX_BINS -1;
    
    uint* inKeys; 
	uint* outKeys;
	uint* inValues;
	uint* outValues;

	for( unsigned short shift = 0 ; shift < 32; shift += RADIX_SHIFT ) // udacians: replace 0 by 16 for MSB16_hack
	{
		inKeys = (toggle) ? h_outputKeys : h_inputKeys;
		outKeys = (toggle) ? h_inputKeys : h_outputKeys;
		inValues = (toggle) ? h_outputValues : h_inputValues;
		outValues = (toggle) ? h_inputValues  : h_outputValues;
		toggle = 1 - toggle;

	// 1st step: accumulate without sort : histogram
		uint bins[ RADIX_BINS ] = { 0 };
		for( uint i = 0; i < numElems; i ++ )
		{
			unsigned short usBin = (inKeys[ i ] >> shift) & radixMask;
			(bins[ usBin ]) ++;
		}

	// 2nd step: scan exclusive
		uint sum = 0;
		for( uint i = 0; i < RADIX_BINS; i ++ )
		{
			uint tmp = bins[ i ];
			bins[ i ] = sum;
			sum += tmp;
		}

	// 3rd step: scatter keys ( and values) to new positions
		for( uint i = 0; i < numElems; i ++ )
		{
			uint key = inKeys[ i ];
			unsigned short usBin = (key >> shift) & radixMask;
			uint newPosition = bins[ usBin ];
			outKeys[ newPosition ] = key;
			if( kind == keysAndValues ) outValues[ newPosition ] = inValues[ i ];
			(bins[ usBin ]) ++;
		}
	// note: in the end bins[] holds an inclusive scan
	}

	return toggle;
}

// --------------------------------------------------------
// The selection algorithm for a partial sort.
// For sure there a faster solutions to identify the nth_element, a threshold,
// but I would like to show the usage of a sort of a sample of the set
/*
A sample of the input keys is full 32 bit radix sorted and used to determine a threshold from a quantile.
With that threshold the input set is reduced / compacted.
Only those above the threshold (hopefully less then quantileCircaLen) are sorted and given to the reference.
*/

uint selectQuantile(	uint* h_inputVals, 
						uint* h_inputPos, 
						uint* h_outputVals, 
						uint* h_outputPos, 
						uint numElems,
						uint quantileCircaLen )
{
// sampling
	uint step = 101; // we guess from Nth of the set / vector
	uint subLength = 0;
	for( uint i = 0 ; i < numElems ; i += step )
    {
        h_outputVals[ subLength ] = h_inputVals[ i ];
		subLength++;
    }

// sorting
	// use both output buffers as interim buffers and keep input buffers as they are
	uint toggle = radix256Sort< keysOnly >( h_outputVals, NULL, h_outputPos, NULL, subLength );

// threshold guessing
	uint threshIndex = subLength -(quantileCircaLen / step) -1;
	// threshIndex *= 3; // empirical: 3x fits better to poisson distribution ?
	uint tailThreshold = ( toggle == 1 ) ? h_outputPos[ threshIndex ] : h_outputVals[ threshIndex ];

	// printf( "samplingStep=%d threshIndex=%d tailThreshold=0x%08x\n", step, threshIndex, tailThreshold );

// selectionand compact
	uint numTailElems = 0;
	for( uint i = 0; i < numElems ; i++ )
	{
		if( h_inputVals[ i ] >= tailThreshold )
		{
			h_outputVals[ numTailElems ] = h_inputVals[ i ];
			h_outputPos[ numTailElems ] = h_inputPos[ i ];
			numTailElems ++;
		}
	}

	return numTailElems;
}

// support functions -----------------------------------

void fill_inputVals( uint* vec, uint nElems )
{
	// example: fill vec with floats from [0..5(, but store them as uint.
	float* fvec = (float*)vec;
	float fRandMax = (float)(RAND_MAX) / 5.0f;
	for( uint i = 0; i < nElems; i ++ )
	{
		fvec[ i ] = (float)rand() * fRandMax;
	}
	
	if( nElems <= 16 ) // for a small debug and validation
	{
		printf( "IN:  " );
		for( uint i = 0; i < nElems; i++ ) printf( "%08x ", vec[ i ] );
		printf( "\n" );
	}
}

uint check_ouputValsSorted( uint* vec, uint nElems )
{
	if( nElems <= 16 ) // for a small debug and validation
	{
		printf( "OUT: " );
		for( uint i = 0; i < nElems; i++ ) printf( "%08x ", vec[ i ] );
		printf( "\n" );
	}

	uint valBelow = vec[ 0 ];
	for( uint i = 1; i < nElems; i ++ )
	{
		uint val = vec[ i ];
		if( valBelow > val ) return 2;
		valBelow = val;
	}
	return 0;
}

// main ------------------------------------------------

void _tmain(int argc, _TCHAR* argv[])
{
	int err = 0;

	size_t numElems = 220480;
	// size_t numElems = 16; // in case of debug, use a small set
	uint* h_inputVals = (uint*) malloc( numElems * sizeof( uint ));
	uint* h_inputPos = (uint*) malloc( numElems * sizeof( uint ));
	uint* h_outputVals = (uint*) malloc( numElems * sizeof( uint ));
	uint* h_outputPos = (uint*) malloc( numElems * sizeof( uint ));

	uint* inKeys = h_inputVals;
	uint* outKeys = h_outputVals;
	uint* inValues = h_inputPos;
	uint* outValues = h_outputPos;

	if( (inKeys == NULL) || (outKeys == NULL) || (inValues == NULL) || (outValues == NULL) ) 
		{ err = 1; goto fini; }

	fill_inputVals( inKeys, numElems );

#ifdef PERFORMANCE_CPU
	LARGE_INTEGER StartingTime, EndingTime;
	LARGE_INTEGER Frequency;

	QueryPerformanceFrequency(&Frequency); 
	QueryPerformanceCounter(&StartingTime);
#endif

	uint tailElems = numElems;

#ifdef PARTIAL_SORT
	// select only the interesting tail (of a eventually full sort) of about 1024 elems
	tailElems = selectQuantile( inKeys, inValues, outKeys, outValues, numElems, 1024 );
	inKeys = h_outputVals;
	outKeys = h_inputVals;
	inValues = h_outputPos;
	outValues = h_inputPos;
#endif

	uint toggle = radix256Sort< keysAndValues>( inKeys, inValues, outKeys, outValues, tailElems );

#ifdef PERFORMANCE_CPU
	QueryPerformanceCounter(&EndingTime);
	double elapsedTime_ms = (EndingTime.QuadPart - StartingTime.QuadPart) * 1000.0 / Frequency.QuadPart;

	printf( "Duration=%f [ms]\n", elapsedTime_ms );
#endif

	printf( "%s RadixSort_%d of top %d key-values of %d elems\n", (tailElems == numElems) ? "full" : "partial",
		RADIX_BINS, tailElems, numElems );

	err = check_ouputValsSorted( (toggle == 1) ? outKeys : inKeys, tailElems );

fini:
	if( h_inputVals ) free( h_inputVals );
	if( h_inputPos ) free( h_inputPos );
	if( h_outputVals ) free( h_outputVals );
	if( h_outputPos ) free( h_outputPos );

	if( err == 0) printf( "test vector is sorted\n" ); else printf( "error=%d\n", err );
}
