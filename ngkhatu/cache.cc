/*******************************************************
                          cache.cc
                  Amro Awad & Yan Solihin
                           2013
                {ajawad,solihin}@ece.ncsu.edu
********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <cmath>
#include "cache.h"

#define read_from_console 1

using namespace std;

Cache::Cache(int s,int a,int b, int c_id, Bus* ptr_to_Bus)
{
   ulong i, j;
   reads = readMisses = writes = 0;
   writeMisses = writeBacks = currentCycle = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));
   log2Blk    = (ulong)(log2(b));

	// Cache ID
	cache_id = c_id;

    // Cache now has a pointer to the bus
    my_Bus_Instance=ptr_to_Bus;

	// Coherence counters
	num_transit_inv_to_exc = 0;	num_transit_inv_to_shd = 0;	num_transit_mod_to_shd = 0;
	num_transit_exc_to_shd = 0;	num_transit_shd_to_mod = 0;	num_transit_inv_to_mod = 0;
	num_transit_exc_to_mod = 0;	num_transit_own_to_mod = 0;	num_transit_mod_to_own = 0;
	num_transit_shd_to_inv = 0; num_transfers_cache_to_cache = 0; num_interventions = 0;
	num_invalidations = 0; num_flushes = 0; tagMask =0;

   for(i=0;i<log2Sets;i++) { tagMask <<= 1; tagMask |= 1; }

   /**create a two dimentional cache, sized as cache[sets][assoc]**/
   cache_Instance = new cacheBlock*[sets];
   for(i=0; i<sets; i++)
   {
      cache_Instance[i] = new cacheBlock[assoc];
      // CacheBlock constructor also sets state to INV
      for(j=0; j<assoc; j++) cache_Instance[i][j].invalidate();
   }

} // End Constructor

/*find a victim, move it to MRU position*/
cacheBlock* Cache::findBlockToReplace(ulong addr)
{
   cacheBlock* victim = get_LeastRecentlyUsed_Block(addr);
   updateBlock_Least_to_Most_Recently_Used(victim);

   return (victim);
}

/*allocate a new line*/
cacheBlock* Cache::fillBlock(ulong addr)
{

   ulong tag;

   cacheBlock* victim = findBlockToReplace(addr);
   assert(victim != 0);

   if(victim->getFlags() == DIRTY) writeBack(addr);

   tag = calcTag(addr);
   victim->setTag(tag);
   victim->setFlags(VALID);
   /**note that this cache block has been already
      upgraded to MRU in the previous function (findBlockToReplace)**/

   return victim;
}

/*look up line*/
cacheBlock* Cache::findBlock(ulong addr)
{
   ulong i, j, tag, pos;

   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);

   for(j=0; j<assoc; j++)
	if(cache_Instance[i][j].isValid())
        if(cache_Instance[i][j].getTag() == tag)
		{
		     pos = j; break;
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache_Instance[i][pos]);
}
/*return an invalid Block as LRU, if any, otherwise return LRU Block*/
cacheBlock* Cache::get_LeastRecentlyUsed_Block(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);

   for(j=0;j<assoc;j++)
   {
      if(cache_Instance[i][j].isValid() == 0) return &(cache_Instance[i][j]);
   }
   for(j=0;j<assoc;j++)
   {
	 if(cache_Instance[i][j].getSeq() <= min) { victim = j; min = cache_Instance[i][j].getSeq();}
   }
   assert(victim != assoc);

   return &(cache_Instance[i][victim]);
}


/**you might add other parameters to Access() since this function is an entry point to the memory hierarchy (i.e. caches)**/
cacheBlock* Cache::Access(ulong addr,uchar op)
{
currentCycle++;/*per cache global counter to maintain LRU order
			among cache ways, updated on every cache access*/
// Increment read or write
if(op == 'w') writes++;
else          reads++;

// Look for Tag Match in the address to check if stored in cache
// If doesn't exist in cache, returns NULL
cacheBlock* local_cache_Block = findBlock(addr);

/**since it's a hit, update LRU and update dirty flag**/
if(local_cache_Block != NULL) updateBlock_Least_to_Most_Recently_Used(local_cache_Block);

//MISS
else
{
    if(op == 'w') writeMisses++;
    else readMisses++;
    local_cache_Block = fillBlock(addr);
}
if(op == 'w') local_cache_Block->setFlags(DIRTY);

return local_cache_Block;

} //End Access()


/*upgrade LRU line to be MRU line*/
void Cache::updateBlock_Least_to_Most_Recently_Used(cacheBlock *line)
{
  line->setSeq(currentCycle);
}


void Cache::printStats(int num_processors)
{
	FILE * logFile;
	char *outFilename =  (char *)malloc(60);

if(!read_from_console)
    {

	if(cache_protocol==MSI)sprintf(outFilename, "../Output/MSI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MESI)sprintf(outFilename, "../Output/MESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MOESI)sprintf(outFilename, "../Output/MOESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);

    }
else
    {

	if(cache_protocol==MSI)sprintf(outFilename, "MSI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MESI)sprintf(outFilename, "MESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MOESI)sprintf(outFilename, "MOESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);

    }
    logFile = fopen(outFilename, "a");
	/****ouput file format**************/
	fprintf(logFile, "===== Simulation results (Cache_%lu)", cache_id);
	fprintf(logFile, "      =====\n");
	fprintf(logFile, "01. number of reads:                            %lu\n", reads);
	fprintf(logFile, "02. number of read misses:                      %lu\n", readMisses);
	fprintf(logFile, "03. number of writes:                           %lu\n", writes);
	fprintf(logFile, "04. number of write misses:                     %lu\n", writeMisses);
	fprintf(logFile, "05. number of write backs:                      %lu\n", writeBacks);
	fprintf(logFile, "06. number of invalid to exclusive (INV->EXC):  %lu\n", num_transit_inv_to_exc);
	fprintf(logFile, "07. number of invalid to shared (INV->SHD):     %lu\n", num_transit_inv_to_shd);
	fprintf(logFile, "08. number of modified to shared (MOD->SHD):    %lu\n", num_transit_mod_to_shd);
	fprintf(logFile, "09. number of exclusive to shared (EXC->SHD):   %lu\n", num_transit_exc_to_shd);
	fprintf(logFile, "10. number of shared to modified (SHD->MOD):    %lu\n", num_transit_shd_to_mod);
	fprintf(logFile, "11. number of invalid to modified (INV->MOD):   %lu\n", num_transit_inv_to_mod);
	fprintf(logFile, "12. number of exclusive to modified (EXC->MOD): %lu\n", num_transit_exc_to_mod);
	fprintf(logFile, "13. number of owned to modified (OWN->MOD):     %lu\n", num_transit_own_to_mod);
	fprintf(logFile, "14. number of modified to owned (MOD->OWN):     %lu\n", num_transit_mod_to_own);
	fprintf(logFile, "15. number of shared to invalid (SHD->INV):     %lu\n", num_transit_shd_to_inv);
	fprintf(logFile, "16. number of cache to cache transfers:         %lu\n", num_transfers_cache_to_cache);
	fprintf(logFile, "17. number of interventions:                    %lu\n", num_interventions);
	fprintf(logFile, "18. number of invalidations:                    %lu\n", num_invalidations);
	fprintf(logFile, "19. number of flushes:                          %lu\n", num_flushes);

	delete outFilename;
	fclose(logFile);

	return ;
}


void Cache::printData_for_Excel(int num_processors)
{
	FILE * logFile;
	char *outFilename =  (char *)malloc(60);

	if(cache_protocol==MSI)sprintf(outFilename, "../E_Output/E_MSI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MESI)sprintf(outFilename, "../E_Output/E_MESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);
	else if(cache_protocol==MOESI)sprintf(outFilename, "../E_Output/E_MOESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", (int)size, (int)assoc,(int)lineSize, num_processors);


	logFile = fopen(outFilename, "a");

	/****ouput file format**************/
	fprintf(logFile, "%lu\n", reads);
	fprintf(logFile, "%lu\n", readMisses);
	fprintf(logFile, "%lu\n", writes);
	fprintf(logFile, "%lu\n", writeMisses);
	fprintf(logFile, "%lu\n", writeBacks);
	fprintf(logFile, "%lu\n", num_transit_inv_to_exc);
	fprintf(logFile, "%lu\n", num_transit_inv_to_shd);
	fprintf(logFile, "%lu\n", num_transit_mod_to_shd);
	fprintf(logFile, "%lu\n", num_transit_exc_to_shd);
	fprintf(logFile, "%lu\n", num_transit_shd_to_mod);
	fprintf(logFile, "%lu\n", num_transit_inv_to_mod);
	fprintf(logFile, "%lu\n", num_transit_exc_to_mod);
	fprintf(logFile, "%lu\n", num_transit_own_to_mod);
	fprintf(logFile, "%lu\n", num_transit_mod_to_own);
	fprintf(logFile, "%lu\n", num_transit_shd_to_inv);
	fprintf(logFile, "%lu\n", num_transfers_cache_to_cache);
	fprintf(logFile, "%lu\n", num_interventions);
	fprintf(logFile, "%lu\n", num_invalidations);
	fprintf(logFile, "%lu\n", num_flushes);
	fprintf(logFile, "\n");

	delete outFilename;
	fclose(logFile);

	return ;
}




/*
void Cache::increment_counter(state from, state to)
{
    if(from==INV && to==EXC) num_transit_inv_to_exc++;
    else if(from==INV && to==SHD) num_transit_inv_to_shd++;
    else if(from==MOD && to==SHD) num_transit_mod_to_shd++;
    else if(from==EXC && to==SHD) num_transit_exc_to_shd++;
    else if(from==SHD && to==MOD) num_transit_shd_to_mod++;
    else if(from==INV && to==MOD) num_transit_inv_to_mod++;
    else if(from==EXC && to==MOD) num_transit_exc_to_mod++;
    else if(from==OWN && to==MOD) num_transit_own_to_mod++;
    else if(from==MOD && to==OWN) num_transit_mod_to_own++;
    else if(from==SHD && to==INV) num_transit_shd_to_inv++;

    return;
}
*/
ulong Cache::calcTag(ulong addr) { return (addr >> (log2Blk) );}
