#include <postgres.h>
#include <executor/executor.h>
#include <funcapi.h>
#include <utils/memutils.h>
#include <math.h>


#ifdef PG_MODULE_MAGIC
	PG_MODULE_MAGIC;
#endif

#define MXT_NAME_LENGTH 128

/* 1024 different memory context at most */

#define MXT_NAME_NUMBER 1024

#define INTERVALS_IN_HISTOGRAMM 11

#define ALLOCSET_NUM_FREELISTS 11

#define NUMBER_OF_FIELDS 11

typedef struct AllocBlockData *AllocBlock;
typedef struct AllocChunkData *AllocChunk;
typedef struct AllocSetContext *AllocSet;
typedef void *AllocPointer;


typedef struct MxtStat {
	char* name; // name of the context
	char type;
	unsigned int id; // unique id of the context
	unsigned int parentid; // id of the contexts parent context
	Size initBlockSize;
	Size maxBlockSize;
	Size allocChunkLimit;
	Size nblocks;
	Size totalspace;
	Size freespace;         
	/*
	 * Here is our histogramm of block sizes
	 * It contains 10 intervals from 0 to allocChunkLimit
	 * And count of blocks in the context, allocated this amount of memory
	 * 11th contains count of blocks which have size bigger than allocChunkLimit,  
	*/
	unsigned int  histogramm[INTERVALS_IN_HISTOGRAMM];
} MxtStat;


typedef struct AllocBlockData{
	AllocSet	aset;			/* aset that owns this block */
	AllocBlock	prev;			/* prev block in aset's blocks list, if any */
	AllocBlock	next;			/* next block in aset's blocks list, if any */
	char* freeptr;		/* start of free space in this block */
	char* endptr;			/* end of space in this block */
}AllocBlockData;


typedef struct AllocChunkData{
	Size		size;
#ifdef MEMORY_CONTEXT_CHECKING
	Size		requested_size;
#if MAXIMUM_ALIGNOF > 4 && SIZEOF_VOID_P == 4
	Size		padding;
#endif
#endif	/* MEMORY_CONTEXT_CHECKING */
	void	   *aset;

} AllocChunkData;


typedef struct AllocSetContext{
	MemoryContextData header;						/* Standard memory-context fields */
	AllocBlock	blocks;								/* head of list of blocks in this set */
	AllocChunk	freelist[ALLOCSET_NUM_FREELISTS];	/* free chunk lists */
	Size		initBlockSize;						/* initial block size */
	Size		maxBlockSize;						/* maximum block size */
	Size		nextBlockSize;						/* next block size to allocate */
	Size		allocChunkLimit;					/* effective chunk size limit */
	AllocBlock	keeper;								/* if not NULL, keep this block over resets */
} AllocSetContext;


void _PG_init(void);
void _PG_fini(void);

Datum pg_memorydump(PG_FUNCTION_ARGS);

static void MxtCacheInitialize(void);
static MxtStat* MxtAllocSetStats(MemoryContext context,unsigned int* parentid);
static void MxtMemoryContextStats(MemoryContext context);
static void MxtStatsInternal(MemoryContext context, MxtStat* parentnode);
static int HistogrammDigitsCnt(unsigned int* histogramm);
static char* HistStr(unsigned int* histogramm);
static char** MxtFillValues(MxtStat* mxt_stat);


void _PG_init(void){
}

void _PG_fini(void){
}

PG_FUNCTION_INFO_V1(pg_memorydump);

static HTAB *MxtCache;
static unsigned int cacheid = 0;

Datum 
pg_memorydump(PG_FUNCTION_ARGS)
{
	
	FuncCallContext* funcctx;
	MemoryContext oldcontext;
	HASH_SEQ_STATUS* mxt_status;
	TupleDesc tupdesc;
	AttInMetadata* attinmeta;

	if (SRF_IS_FIRSTCALL())
	{
		
		funcctx = SRF_FIRSTCALL_INIT(); // Initial initialisation
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx); // Switch to the temporal context
		
		MxtCacheInitialize();
        MxtMemoryContextStats(TopMemoryContext);

		/*
		 * Max count of function calls is equal to actual number of contexts
		*/

        funcctx->max_calls = hash_get_num_entries(MxtCache);

		mxt_status = (HASH_SEQ_STATUS*) palloc(sizeof(HASH_SEQ_STATUS));
        hash_seq_init(mxt_status, MxtCache);
        funcctx->user_fctx = (void*) mxt_status;

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Function called in context"
						"that can't accept the record type")));
		}
		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;
		MemoryContextSwitchTo(oldcontext); // Return to original context when allocating transient memory
	}

	funcctx = SRF_PERCALL_SETUP();
	attinmeta = funcctx->attinmeta;

	if(funcctx->call_cntr < funcctx->max_calls)
	{
		
		HASH_SEQ_STATUS* user_mxt_status;
        MxtStat* mxt_stat;
		char** values;
		HeapTuple tuple;
        Datum result = 0;

        user_mxt_status = (HASH_SEQ_STATUS*)funcctx->user_fctx;
        mxt_stat = (MxtStat*)hash_seq_search(user_mxt_status);

        if (mxt_stat)
		{
			values = MxtFillValues(mxt_stat);
			
			tuple = BuildTupleFromCStrings(attinmeta, values);
			result = HeapTupleGetDatum(tuple);
			
			/*
 			 * Now it's time to free the allocated for values array memory
			*/

			for (int i = 0; i < NUMBER_OF_FIELDS; i++)
				pfree(values[i]);
			pfree(values);
		}
		SRF_RETURN_NEXT(funcctx,result);
	}
	else
	{
        /* 
		 * finish seq_search
		*/
		//pfree(mxt_status);
        if (hash_seq_search((HASH_SEQ_STATUS*)funcctx->user_fctx))
            elog(ERROR, "pg_memorydump: leaked scan hash table");
        SRF_RETURN_DONE(funcctx);
    }
}


static void 
MxtCacheInitialize(void)
{
    HASHCTL mxt_ctl;

	cacheid = 0;
    MemSet(&mxt_ctl, 0, sizeof(mxt_ctl));

    /*
     * create memorydump hashtable under SRF multi-call context
	 * KEYSIZE : size in bytes of key (id in our case) (tt's required for tag_hash function)
	 * entrysize : size in bytes of every 'row' in our hash table
	 * tag_hash : preinstalled hash function for fixed-size tag values
	 * hcxt : the memory context, where our hash table will be located
     */

    mxt_ctl.keysize = sizeof(unsigned int);
    mxt_ctl.entrysize = sizeof(MxtStat);
    mxt_ctl.hash = tag_hash;
    mxt_ctl.hcxt = CurrentMemoryContext;
    MxtCache = hash_create("pg_memorydump hash table", MXT_NAME_NUMBER, &mxt_ctl, HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

    if (MxtCache == NULL)
        elog(ERROR, "pg_memorydump: can't create a hash table");
}


static MxtStat* 
MxtAllocSetStats(MemoryContext context,unsigned int* parentid)
{
	unsigned int* cacheptr = &cacheid;;
	AllocSet set = (AllocSet) context;
	MxtStat* mxtstat;
	AllocBlock block;
	Size nblocks = 0;
	Size totalspace = 0;
	Size freespace = 0;

	++cacheid;

	/* 
	 * In our case every single context in the tree won't be found in hash table
	 * So it will be entered as a new element
	*/

	mxtstat = (MxtStat*) hash_search(MxtCache, (void*) cacheptr, HASH_ENTER, NULL);
	
	/*
	 * initialize all the values 0
	*/

	memset(mxtstat->histogramm, 0, INTERVALS_IN_HISTOGRAMM * sizeof(unsigned int));
	for (block = set->blocks; block != NULL; block = block->next)
	{
		
		++nblocks;
		totalspace += block->endptr - ((char*)block);
		freespace += block->endptr - block->freeptr;

		/*
		 * Histogramm Filling.
		 * All sizes that bigger than allocChunkLimit of the context are moving to the last field of our histogramm
		 * Just because block can't be completely empty, in rest cases index is the decremented smallest number, which bigger or equal than ( <size of block> * (INTERVALS_IN_HISTOGRAMM - 1) / <allocChunkLimit> )
		 */
		
		if( block->endptr - ((char*)block) > set->allocChunkLimit )
			++mxtstat->histogramm[INTERVALS_IN_HISTOGRAMM - 1];
		else
			++mxtstat->histogramm[ (int) ceil( ((float) (block->endptr - ((char*)block)) * (INTERVALS_IN_HISTOGRAMM - 1) ) / set->allocChunkLimit ) - 1 ];
	}
	
	switch (set->header.type)
	{
	case T_MemoryContext : 
		mxtstat->type = 'M';
		break;
	case T_AllocSetContext : 
		mxtstat->type = 'A';
		break;
	case T_SlabContext : 
		mxtstat->type = 'S';
		break;
	default : 
		mxtstat->type = 'O';
	}
	mxtstat->name = set->header.name;
	mxtstat->id = cacheid;
	mxtstat->parentid = *parentid;
	mxtstat->initBlockSize = set->initBlockSize;
	mxtstat->maxBlockSize = set->maxBlockSize;
	mxtstat->allocChunkLimit = set->allocChunkLimit;
	mxtstat->nblocks = nblocks;
	mxtstat->totalspace = totalspace;
	mxtstat->freespace = freespace;


	return mxtstat;
}

static void 
MxtMemoryContextStats(MemoryContext context)
{
	MxtStatsInternal(context, NULL);
}


static void 
MxtStatsInternal(MemoryContext context, MxtStat *parentnode) 
{
	MxtStat* node;
	unsigned int parentid;
	
	AssertArg(MemoryContextIsValid(context));
	
	parentid = (parentnode ? parentnode->id : 0);
	node = MxtAllocSetStats(context,&parentid);
	
	for (MemoryContext child = context->firstchild; child != NULL; child = child->nextchild)
       MxtStatsInternal(child, node);
}

/* 
 * Get summary count of single digits in values of our histogramm 
*/
static int 
HistogrammDigitsCnt(unsigned int* histogramm)
{
	int result = 0; // count of single digits
	int value; // temp variable

	for (int i = 0; i < INTERVALS_IN_HISTOGRAMM; i++)
	{
		value = histogramm[i];
		
		/*
		 * Separating of single digits from the whole number
		*/
		
		do 
		{
			result++;
			value /= 10; // bitwise right shift in decimal number system 
		} while (value != 0);
	}
	return result;
}

/*
 * Form C-string from histogramm integer array 
*/
static char* 
HistStr(unsigned int* histogramm) 
{
	
	/*
	 * length is a whole length of our future C-string
	 * including 2 braces, 10 commas and '\0' 
	*/

	int length = HistogrammDigitsCnt(histogramm) + 13;
	char* histstr = (char*)palloc(length * sizeof(char));
	histstr[0] = '{';
	
	int j = 1; // number of symbols, already have been written

	for (int i = 0; i < INTERVALS_IN_HISTOGRAMM; i++) 
	{
		j += sprintf(histstr + j, "%d", histogramm[i]);
		
		/*
		 * Insert ',' after every number in our histogramm expecting the last one
		*/

		if(j == length - 2)
			break;
		histstr[j] = ',';
		++j;
	}
	
	histstr[length - 2] = '}';
	histstr[length - 1] = '\0';
	
	return histstr;
}


/*
 * values is an array of C-strings, which is used for buiding tuple
*/
static char** 
MxtFillValues(MxtStat* mxt_stat) 
{
	char** values;

	values = (char**)palloc( NUMBER_OF_FIELDS * sizeof(char*));
	
	/* 
	 * add extra spaces for '\0' 
	*/

	values[0] = (char*) palloc( (MXT_NAME_LENGTH + 1) * sizeof(char));
	values[1] = (char*) palloc(sizeof(char) + 1 ); 
	
	for (int i = 2; i < NUMBER_OF_FIELDS - 1; i++)
		values[i] = (char*)palloc(16 * sizeof(char));
	
	/*
	 * Walk through all fields of mxtstat struct
	*/

	snprintf(values[0], MXT_NAME_LENGTH, "%s", mxt_stat->name);
	snprintf(values[1], 2, "%c",mxt_stat->type);
	snprintf(values[2], 16, "%u", mxt_stat->id);
	snprintf(values[3], 16, "%u", mxt_stat->parentid);
	snprintf(values[4], 16, "%u", mxt_stat->initBlockSize);
	snprintf(values[5], 16, "%u", mxt_stat->maxBlockSize);
	snprintf(values[6], 16, "%u", mxt_stat->allocChunkLimit);
	snprintf(values[7], 16, "%u", mxt_stat->nblocks);
	snprintf(values[8], 16, "%u", mxt_stat->totalspace);
	snprintf(values[9], 16, "%u", mxt_stat->freespace);
	values[10] = HistStr(mxt_stat->histogramm);
	
	return values;
}