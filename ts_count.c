#include "postgres.h"

#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

static int get_counts(TSVector t, TSQuery q);

extern Datum ts_count(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(ts_count);

Datum
ts_count(PG_FUNCTION_ARGS)
{
    TSVector    txt = PG_GETARG_TSVECTOR(0);
    TSQuery     query = PG_GETARG_TSQUERY(1);
    int         res;

	res = get_counts(txt,query);

    PG_FREE_IF_COPY(txt, 0);
    PG_FREE_IF_COPY(query, 1);
    PG_RETURN_INT32(res);	
}

/* a couple of utility functions imported from tsrank.c */

#define WordECompareQueryItem(e,q,p,i,m) \
    tsCompareString((q) + (i)->distance, (i)->length,   \
                    (e) + (p)->pos, (p)->len, (m))


static WordEntry *
find_wordentry(TSVector t, TSQuery q, QueryOperand *item, int32 *nitem)
{
    WordEntry  *StopLow = ARRPTR(t);
    WordEntry  *StopHigh = (WordEntry *) STRPTR(t);
    WordEntry  *StopMiddle = StopHigh;
    int         difference;

    *nitem = 0;

    /* Loop invariant: StopLow <= item < StopHigh */
    while (StopLow < StopHigh)
    {
        StopMiddle = StopLow + (StopHigh - StopLow) / 2;
        difference = WordECompareQueryItem(STRPTR(t), GETOPERAND(q), StopMiddle, item, false);
        if (difference == 0)
        {
            StopHigh = StopMiddle;
            *nitem = 1;
            break;
        }
        else if (difference > 0)
            StopLow = StopMiddle + 1;
        else
            StopHigh = StopMiddle;
    }

    if (item->prefix)
    {
        if (StopLow >= StopHigh)
            StopMiddle = StopHigh;

        *nitem = 0;

        while (StopMiddle < (WordEntry *) STRPTR(t) &&
               WordECompareQueryItem(STRPTR(t), GETOPERAND(q), StopMiddle, item, true) == 0)
        {
            (*nitem)++;
            StopMiddle++;
        }
    }

    return (*nitem > 0) ? StopHigh : NULL;
}

static int
compareQueryOperand(const void *a, const void *b, void *arg)
{
    char       *operand = (char *) arg;
    QueryOperand *qa = (*(QueryOperand **) a);
    QueryOperand *qb = (*(QueryOperand **) b);

    return tsCompareString(operand + qa->distance, qa->length,
                           operand + qb->distance, qb->length,
                           false);
}

static QueryOperand **
SortAndUniqItems(TSQuery q, int *size)
{
    char       *operand = GETOPERAND(q);
    QueryItem  *item = GETQUERY(q);
    QueryOperand **res,
              **ptr,
              **prevptr;

    ptr = res = (QueryOperand **) palloc(sizeof(QueryOperand *) * *size);

    /* Collect all operands from the tree to res */
    while ((*size)--)
    {
        if (item->type == QI_VAL)
        {
            *ptr = (QueryOperand *) item;
            ptr++;
        }
        item++;
    }

    *size = ptr - res;
    if (*size < 2)
        return res;

    qsort_arg(res, *size, sizeof(QueryOperand **), 
			  compareQueryOperand, (void *) operand);

    ptr = res + 1;
    prevptr = res;

    /* remove duplicates */
    while (ptr - res < *size)
    {
        if (compareQueryOperand((void *) ptr, (void *) prevptr, 
								(void *) operand) != 0)
        {
            prevptr++;
            *prevptr = *ptr;
        }
        ptr++;
    }

    *size = prevptr + 1 - res;
    return res;
}


/* do the real work */

static int
get_counts(TSVector t, TSQuery q)
{
	WordEntry  *entry;
	QueryOperand **item;
	int         size = q->size;
	int         i,
		        nitem,
		        elen, 
		        count  = 0;

	item = SortAndUniqItems(q, &size);
	for (i = 0; i < size; i++)
    {
		entry = find_wordentry(t, q, item[i], &nitem);
		if (entry == NULL)
			continue;
		elen = POSDATALEN(t, entry);
		if (elen == 0)
			count += 1;
		else
			count += elen;
	}

	pfree(item);
	return count;
}
