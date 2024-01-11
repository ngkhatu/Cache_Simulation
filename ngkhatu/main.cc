/*******************************************************
                          main.cc
                  Amro Awad & Yan Solihin
                           2013
                {ajawad,solihin}@ece.ncsu.edu
********************************************************/
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include "cache.h"
#include <cmath>

#define read_from_console 1

using namespace std;

char *fname;

void simulate_MSI(int cache_size, int cache_assoc, int blk_size, int num_processors)
{
	FILE * logFile;
	FILE * inputFile;
	char *outFilename =  (char *)malloc(60);

// Dynamic vars
	char operation;
	int proc_number;
	char address[9];
//	char convert_s[9];

	//*******Output simulator configuration*******
	if(!read_from_console) sprintf(outFilename, "../Output/MSI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	else sprintf(outFilename, "MSI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	logFile = fopen(outFilename, "w");
	fprintf(logFile, "===== 506 SMP Simulator Configuration =====\n");
	fprintf(logFile, "L1_SIZE:                        %i\n", cache_size);
	fprintf(logFile, "L1_ASSOC:                       %i\n", cache_assoc);
	fprintf(logFile, "L1_BLOCKSIZE:                   %i\n", blk_size);
	fprintf(logFile, "NUMBER OF PROCESSORS:           %i\n", num_processors);
	fprintf(logFile, "COHERENCE PROTOCOL:             MSI\n");
	fprintf(logFile, "TRACE FILE:                     %s\n", fname);
	fclose(logFile);

//*****create array of caches and instantiate Bus**********//
	Cache* cacheArray[num_processors];
    Bus* main_Bus_Instance = new Bus;

	for(int i = 0; i < num_processors; i++)
    {
        cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size, i, main_Bus_Instance);
        cacheArray[i]->setProtocol((protocol)MSI);
    }


//read trace file
	inputFile = fopen (fname,"r");
	if(inputFile == 0)
	{
		printf("Trace file problem\n");
		exit(0);
	}

// Start sim
	while(!feof(inputFile))
	{
		unsigned long ul_addr;


		// Line format is (processor#,operation,address)
		fscanf(inputFile, "%i %c %s\n", &proc_number, &operation, address);
		//printf("Processor Number:%i, Operation:%c, Address: %s\n", (proc_number%num_processors), operation, address);

		istringstream iss(address);
		iss >> hex >> ul_addr;
/*		sprintf(convert_s, "%x", ul_addr);
		if (strcmp(convert_s, address) != 0) printf("hex mismatch error! address:%s,converted%s\n", address, convert_s);
*/
		//*****propagate each request down through memory hierarchy




		//cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->Access(ul_addr, operation);
		cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->findBlock(ul_addr);

        cacheArray[proc_number%num_processors]->currentCycle++;
		if(operation=='r') cacheArray[proc_number%num_processors]->reads++;                                                 // READS++
        else if(operation=='w')cacheArray[proc_number%num_processors]->writes++;                                            //WRITES++

        state local_block_state;
        if(local_cache_Block!=NULL) local_block_state = local_cache_Block->getState();

        if(local_cache_Block==NULL|| local_block_state==INV)                                                                           //Suffer MISS
            {
                cacheBlock* replaced_block;
                if(operation=='r')
                    {
                        cacheArray[proc_number%num_processors]->readMisses++;
                        if(local_cache_Block==NULL) replaced_block = cacheArray[proc_number%num_processors]->findBlockToReplace(ul_addr);
                        else if(local_cache_Block!=NULL)
                            {
                                assert(local_block_state==INV);
                                replaced_block=local_cache_Block;
                            }
                        if(replaced_block->getState()==MOD)
                            {
                                assert(replaced_block->getFlags()==DIRTY);
                                cacheArray[proc_number%num_processors]->writeBack(ul_addr);
                            }

                        main_Bus_Instance->postBusRd(ul_addr);

                        // Update remote caches-->BusRd
                        for(int i=0;i<num_processors;i++)
                            {
                                if(i == (proc_number%num_processors)) continue;
                                ulong bus_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* read_update_Block = cacheArray[i]->findBlock(bus_addr);
                                if(read_update_Block!=NULL)
                                    {
                                        state read_update_block_state = read_update_Block->getState();
                                        if(read_update_block_state==MOD)
                                            {
                                                read_update_Block->setState(SHD);
                                                cacheArray[i]->num_interventions++;
                                                cacheArray[i]->num_transit_mod_to_shd++;
                                                read_update_Block->setFlags(VALID);
                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;
                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); // End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);

                        // Update new block after flush received
                        //assert(main_Bus_Instance->isFlush_on_Bus());
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        replaced_block->setTag(addr_tag);
                        replaced_block->setFlags(VALID);
                        cacheArray[proc_number%num_processors]->num_transit_inv_to_shd++;
                        replaced_block->setState(SHD);

                    }                                                                           // End read on MISS
                else if(operation=='w')
                    {
                        cacheArray[proc_number%num_processors]->writeMisses++;
                        cacheArray[proc_number%num_processors]->num_transit_inv_to_mod++;
                        if(local_cache_Block==NULL) replaced_block = cacheArray[proc_number%num_processors]->findBlockToReplace(ul_addr);
                        else if(local_cache_Block!=NULL)
                            {
                                assert(local_block_state==INV);
                                replaced_block=local_cache_Block;
                            }
                        if(replaced_block->getState()==MOD)
                            {
                                assert(replaced_block->getFlags()==DIRTY);
                                cacheArray[proc_number%num_processors]->writeBack(ul_addr);
                            }
                        main_Bus_Instance->postBusRdX(ul_addr);

                        // Update remote caches
                        for(int i=0; i<num_processors;i++)
                            {
                                if(i== (proc_number%num_processors)) continue;
                                ulong bus_write_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* write_update_Block = cacheArray[i]->findBlock(bus_write_addr);
                                if(write_update_Block!=NULL)
                                    {
                                        state write_update_block_state = write_update_Block->getState();
                                        if(write_update_block_state==MOD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;
                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;
                                            }
                                        else if(write_update_block_state==SHD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;
                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); //End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);

                        // Update new block after flush received
                        //assert(main_Bus_Instance->isFlush_on_Bus());
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        replaced_block->setTag(addr_tag);
                        replaced_block->setFlags(DIRTY);
                        replaced_block->setState(MOD);

                    }                                                                       // End write on MISS
            }   // End MISS logic

        else if(local_cache_Block!=NULL)                                                                           // Cache HIT
            {
                cacheArray[proc_number%num_processors]->updateBlock_Least_to_Most_Recently_Used(local_cache_Block);
                if(operation=='w')
                    {
                        if(local_cache_Block->getState()==SHD)
                            {
                                main_Bus_Instance->postBusRdX(ul_addr);

                                //Update Remote Caches
                                for(int i=0; i<num_processors; i++)
                                    {
                                        if(i==(proc_number%num_processors)) continue;
                                        ulong hit_bus_write_addr = main_Bus_Instance->getSignalAddr();
                                        cacheBlock* write_update_Block_hit = cacheArray[i]->findBlock(hit_bus_write_addr);
                                        if(write_update_Block_hit!=NULL)
                                            {
                                                state local_state = write_update_Block_hit->getState();
                                                if(local_state==MOD)
                                                    {
                                                        write_update_Block_hit->setFlags(INVALID);
                                                        write_update_Block_hit->setState(INV);
                                                        cacheArray[i]->num_invalidations++;
                                                        main_Bus_Instance->flush_to_Bus();
                                                        cacheArray[i]->num_flushes++;
                                                    }
                                                else if(local_state==SHD)
                                                    {
                                                        write_update_Block_hit->setFlags(INVALID);
                                                        write_update_Block_hit->setState(INV);
                                                        cacheArray[i]->num_invalidations++;
                                                        cacheArray[i]->num_transit_shd_to_inv++;
                                                    }
                                            }
                                    } // End
                                main_Bus_Instance->clearBus();
                                assert(main_Bus_Instance->getBusSignal()==clear);

                                // Update new block after flush received
                                //assert(main_Bus_Instance->isFlush_on_Bus());
                                main_Bus_Instance->flush_received();
                                ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                                local_cache_Block->setTag(addr_tag);
                                local_cache_Block->setFlags(DIRTY);
                                local_cache_Block->setState(MOD);
                                cacheArray[proc_number%num_processors]->num_transit_shd_to_mod++;
                            } // SHD state when HIT
                        assert(local_cache_Block->getState()!=INV);
                    } // End write on Hit
            }   // End HIT logic
    } // End sim

	// Close input file
	fclose(inputFile);

	//print out all caches' statistics
	for(int i=0; i<num_processors; i++) cacheArray[i]->printStats(num_processors);
	if(!read_from_console) for(int i=0; i<num_processors; i++) cacheArray[i]->printData_for_Excel(num_processors);

	// Deallocate Cache Array
	for(int i=0; i<num_processors; i++) delete cacheArray[i];
	// Deallocate string
	delete outFilename;
	delete main_Bus_Instance;

	return;

}   // End simulate_MSI()

void simulate_MESI(int cache_size, int cache_assoc, int blk_size, int num_processors)
{
	FILE * logFile;
	FILE * inputFile;
	char *outFilename =  (char *)malloc(60);

// Dynamic vars
	char operation;
	int proc_number;
	char address[9];
//	char convert_s[9];

	//*******Output simulator configuration*******
		if(!read_from_console) sprintf(outFilename, "../Output/MESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	else sprintf(outFilename, "MESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	logFile = fopen(outFilename, "w");
	fprintf(logFile, "===== 506 SMP Simulator Configuration =====\n");
	fprintf(logFile, "L1_SIZE:                        %i\n", cache_size);
	fprintf(logFile, "L1_ASSOC:                       %i\n", cache_assoc);
	fprintf(logFile, "L1_BLOCKSIZE:                   %i\n", blk_size);
	fprintf(logFile, "NUMBER OF PROCESSORS:           %i\n", num_processors);
	fprintf(logFile, "COHERENCE PROTOCOL:             MESI\n");
	fprintf(logFile, "TRACE FILE:                     %s\n", fname);
	fclose(logFile);

//*****create array of caches and instantiate Bus**********//
	Cache* cacheArray[num_processors];
    Bus* main_Bus_Instance = new Bus;

	for(int i = 0; i < num_processors; i++)
    {
        cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size, i, main_Bus_Instance);
        cacheArray[i]->setProtocol((protocol)MESI);
    }

//read trace file
	inputFile = fopen (fname,"r");
	if(inputFile == 0)
	{
		printf("Trace file problem\n");
		exit(0);
	}

// Start sim
	while(!feof(inputFile))
	{
		unsigned long ul_addr;


		// Line format is (processor#,operation,address)
		fscanf(inputFile, "%i %c %s\n", &proc_number, &operation, address);
		//printf("Processor Number:%i, Operation:%c, Address: %s\n", (proc_number%num_processors), operation, address);

		istringstream iss(address);
		iss >> hex >> ul_addr;
/*		sprintf(convert_s, "%x", ul_addr);
		if (strcmp(convert_s, address) != 0) printf("hex mismatch error! address:%s,converted%s\n", address, convert_s);
*/
		//cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->Access(ul_addr, operation);
		cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->findBlock(ul_addr);

        cacheArray[proc_number%num_processors]->currentCycle++;
		if(operation=='r') cacheArray[proc_number%num_processors]->reads++;                                                 // READS++
        else if(operation=='w')cacheArray[proc_number%num_processors]->writes++;                                            //WRITES++

        state local_block_state;
        if(local_cache_Block!=NULL) local_block_state = local_cache_Block->getState();

        if(local_cache_Block==NULL|| local_block_state==INV)                                                                           //Suffer MISS
            {
                cacheBlock* replaced_block;

                if(operation=='r')
                    {
                        cacheArray[proc_number%num_processors]->readMisses++;
                        if(local_cache_Block==NULL) // Find a new Block to replace
                            {
                                replaced_block = cacheArray[proc_number%num_processors]->findBlockToReplace(ul_addr);
                                if(replaced_block->getState()==MOD)
                                    {
                                        assert(replaced_block->getFlags()==DIRTY);
                                        cacheArray[proc_number%num_processors]->writeBack(ul_addr);
                                    }
                            }
                        else if(local_cache_Block!=NULL) //Block exists in INVALID state with INV flag
                            {
                                assert(local_block_state==INV);
                                replaced_block=local_cache_Block;
                            }

                        assert(main_Bus_Instance->getBusSignal()==clear);
                        main_Bus_Instance->postBusRd(ul_addr);

                        // Update remote caches-->BusRd
                        for(int i=0;i<num_processors;i++)
                            {
                                if(i == (proc_number%num_processors)) continue;
                                ulong bus_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* read_update_Block = cacheArray[i]->findBlock(bus_addr);
                                if(read_update_Block!=NULL)
                                    {
                                        main_Bus_Instance->post_Copies_Exist();
                                        state read_update_block_state = read_update_Block->getState();
                                        if(read_update_block_state==MOD)
                                            {
                                                read_update_Block->setState(SHD);
                                                cacheArray[i]->num_interventions++;
                                                cacheArray[i]->num_transit_mod_to_shd++;
                                                read_update_Block->setFlags(VALID);
                                                main_Bus_Instance->flush_to_Bus();
                                                //cacheArray[i]->num_transfers_cache_to_cache++; //MIGHT BE TEMPORARY
                                                cacheArray[i]->num_flushes++;
                                            }
                                        else if(read_update_block_state==SHD)
                                            {

                                                main_Bus_Instance->Flush_Opt_to_Bus();
                                                //next state is SHD
                                            }
                                        else if(read_update_block_state==EXC)
                                            {
                                                main_Bus_Instance->Flush_Opt_to_Bus();
                                                cacheArray[i]->num_interventions++;
                                                cacheArray[i]->num_transit_exc_to_shd++;
                                                read_update_Block->setState(SHD);
                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); // End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);

                        // Update new block after flush received
                        //assert(main_Bus_Instance->isFlush_on_Bus());

                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        if(main_Bus_Instance->isFlushOpt_on_Bus())
                            {
                                main_Bus_Instance->Flush_Opt_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }

                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        replaced_block->setTag(addr_tag);
                        replaced_block->setFlags(VALID);

                        if(main_Bus_Instance->Copies_Exist()==1)
                            {
                                cacheArray[proc_number%num_processors]->num_transit_inv_to_shd++;
                                replaced_block->setState(SHD);
                            }
                        else if(main_Bus_Instance->Copies_Exist()==0)
                            {
                                replaced_block->setState(EXC);
                                cacheArray[proc_number%num_processors]->num_transit_inv_to_exc++;
                            }
                        main_Bus_Instance->post_no_Copies_Exist(); //Clear copies flag


                    }                                                                           // End read on MISS
                else if(operation=='w')
                    {
                        cacheArray[proc_number%num_processors]->writeMisses++;
                        cacheArray[proc_number%num_processors]->num_transit_inv_to_mod++;
                        if(local_cache_Block==NULL) replaced_block = cacheArray[proc_number%num_processors]->findBlockToReplace(ul_addr);
                        else if(local_cache_Block!=NULL)
                            {
                                assert(local_block_state==INV);
                                replaced_block=local_cache_Block;
                            }
                        if(replaced_block->getState()==MOD)
                            {
                                assert(replaced_block->getFlags()==DIRTY);
                                cacheArray[proc_number%num_processors]->writeBack(ul_addr);
                            }


                        assert(main_Bus_Instance->getBusSignal()==clear);
                        main_Bus_Instance->postBusRdX(ul_addr);

                        // Update remote caches
                        for(int i=0; i<num_processors;i++)
                            {
                                if(i== (proc_number%num_processors)) continue;
                                ulong bus_write_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* write_update_Block = cacheArray[i]->findBlock(bus_write_addr);
                                if(write_update_Block!=NULL)
                                    {
                                        state write_update_block_state = write_update_Block->getState();
                                        if(write_update_block_state==MOD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;
                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;
                                            }
                                        else if(write_update_block_state==SHD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                //FlushOpt
                                                main_Bus_Instance->Flush_Opt_to_Bus();
                                                cacheArray[i]->num_invalidations++;
                                            }
                                        else if(write_update_block_state==EXC)
                                            {
                                                //FlushOpt
                                                main_Bus_Instance->Flush_Opt_to_Bus();
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); //End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);

                        // Update new block after flush received
                        //assert(main_Bus_Instance->isFlush_on_Bus());
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->Flush_Opt_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        replaced_block->setTag(addr_tag);
                        replaced_block->setFlags(DIRTY);
                        //cacheArray[proc_number%num_processors]->num_transit_shd_to_mod++; <--?
                        replaced_block->setState(MOD);

                    }                                                                       // End write on MISS
            }   // End MISS logic

        else if(local_cache_Block!=NULL)                                                                           // Cache HIT
            {
                cacheArray[proc_number%num_processors]->updateBlock_Least_to_Most_Recently_Used(local_cache_Block);
                if(operation=='w')
                    {
                        if(local_cache_Block->getState()==SHD)
                            {
                                assert(main_Bus_Instance->getBusSignal()==clear);
                                //main_Bus_Instance->postBusRdX(ul_addr);
                                main_Bus_Instance->postBusUpgr(ul_addr);

                                //Update Remote Caches
                                for(int i=0; i<num_processors; i++)
                                    {
                                        if(i==(proc_number%num_processors)) continue;
                                        ulong hit_bus_write_addr = main_Bus_Instance->getSignalAddr();
                                        cacheBlock* write_update_Block_hit = cacheArray[i]->findBlock(hit_bus_write_addr);
                                        if(write_update_Block_hit!=NULL)
                                            {
                                                state local_state = write_update_Block_hit->getState();
                                                if(local_state==MOD)
                                                    {
                                                        write_update_Block_hit->setFlags(INVALID);
                                                        write_update_Block_hit->setState(INV);
                                                        cacheArray[i]->num_invalidations++;
                                                        main_Bus_Instance->flush_to_Bus();
                                                        cacheArray[i]->num_flushes++;
                                                    }
                                                else if(local_state==SHD)
                                                    {
                                                        write_update_Block_hit->setFlags(INVALID);
                                                        write_update_Block_hit->setState(INV);
                                                        cacheArray[i]->num_invalidations++;
                                                        cacheArray[i]->num_transit_shd_to_inv++;
                                                    }
                                            }
                                    } // End
                                main_Bus_Instance->clearBus();
                                assert(main_Bus_Instance->getBusSignal()==clear);

                                // Update new block after flush received
                                //assert(main_Bus_Instance->isFlush_on_Bus());
                                main_Bus_Instance->flush_received();
                                ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                                local_cache_Block->setTag(addr_tag);
                                local_cache_Block->setFlags(DIRTY);
                                local_cache_Block->setState(MOD);
                                cacheArray[proc_number%num_processors]->num_transit_shd_to_mod++;
                            } // SHD state when HIT

                        else if(local_cache_Block->getState()==EXC)
                            {
                                if(operation=='w')
                                    {
                                        local_cache_Block->setState(MOD);
                                        cacheArray[proc_number%num_processors]->num_transit_exc_to_mod++;
                                        local_cache_Block->setFlags(DIRTY);
                                    }

                            }
                        assert(local_cache_Block->getState()!=INV);
                    } // End write on Hit
            }   // End HIT logic
    } // End sim

	// Close input file
	fclose(inputFile);

	//print out all caches' statistics
	for(int i=0; i<num_processors; i++) cacheArray[i]->printStats(num_processors);
	if(!read_from_console)for(int i=0; i<num_processors; i++) cacheArray[i]->printData_for_Excel(num_processors);

	// Deallocate Cache Array
	for(int i=0; i<num_processors; i++) delete cacheArray[i];
	// Deallocate string
	delete outFilename;
	delete main_Bus_Instance;

	return;

}

void simulate_MOESI(int cache_size, int cache_assoc, int blk_size, int num_processors)
{
    FILE * logFile;
	FILE * inputFile;
	char *outFilename =  (char *)malloc(60);

// Dynamic vars
	char operation;
	int proc_number;
	char address[9];
//	char convert_s[9];

	//*******Output simulator configuration*******
		if(!read_from_console) sprintf(outFilename, "../Output/MOESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	else sprintf(outFilename, "MOESI_Csize%i_Cassoc%i_blksize_%i_numproc_%i", cache_size, cache_assoc, blk_size, num_processors);
	logFile = fopen(outFilename, "w");
	fprintf(logFile, "===== 506 SMP Simulator Configuration =====\n");
	fprintf(logFile, "L1_SIZE:                        %i\n", cache_size);
	fprintf(logFile, "L1_ASSOC:                       %i\n", cache_assoc);
	fprintf(logFile, "L1_BLOCKSIZE:                   %i\n", blk_size);
	fprintf(logFile, "NUMBER OF PROCESSORS:           %i\n", num_processors);
	fprintf(logFile, "COHERENCE PROTOCOL:             MOESI\n");
	fprintf(logFile, "TRACE FILE:                     %s\n", fname);
	fclose(logFile);

//*****create array of caches and instantiate Bus**********
	Cache* cacheArray[num_processors];
    Bus* main_Bus_Instance = new Bus;

	for(int i = 0; i < num_processors; i++)
    {
        cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size, i, main_Bus_Instance);
        cacheArray[i]->setProtocol((protocol)MOESI);
    }

//read trace file
	inputFile = fopen (fname,"r");
	if(inputFile == 0)
	{
		printf("Trace file problem\n");
		exit(0);
	}

// Start sim
	while(!feof(inputFile))
	{
		unsigned long ul_addr;


		// Line format is (processor#,operation,address)
		fscanf(inputFile, "%i %c %s\n", &proc_number, &operation, address);
		//printf("Processor Number:%i, Operation:%c, Address: %s\n", (proc_number%num_processors), operation, address);

		istringstream iss(address);
		iss >> hex >> ul_addr;
/*		sprintf(convert_s, "%x", ul_addr);
		if (strcmp(convert_s, address) != 0) printf("hex mismatch error! address:%s,converted%s\n", address, convert_s);
*/
		//cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->Access(ul_addr, operation);
		cacheBlock* local_cache_Block = cacheArray[proc_number%num_processors]->findBlock(ul_addr);

        cacheArray[proc_number%num_processors]->currentCycle++;
		if(operation=='r') cacheArray[proc_number%num_processors]->reads++;                                                 // READS++
        else if(operation=='w')cacheArray[proc_number%num_processors]->writes++;                                            //WRITES++

        state block_state;
        if(local_cache_Block!=NULL) block_state = local_cache_Block->getState();




        if(local_cache_Block==NULL || block_state==INV)                                                                           //Suffer MISS
            {

                if(local_cache_Block==NULL) // Find a new Block to replace
                {
                    local_cache_Block = cacheArray[proc_number%num_processors]->findBlockToReplace(ul_addr);
                    if(local_cache_Block->getState()==MOD) cacheArray[proc_number%num_processors]->writeBack(ul_addr);
                }

                if(operation=='r')
                    {
                        cacheArray[proc_number%num_processors]->readMisses++;

            //************************************* POST BUS READ**********************************************
                        assert(main_Bus_Instance->getBusSignal()==clear);
                        main_Bus_Instance->postBusRd(ul_addr);
            // Update remote caches-->BusRd
                        for(int i=0;i<num_processors;i++)
                            {
                                if(i == (proc_number%num_processors)) continue;
                                ulong bus_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* read_update_Block = cacheArray[i]->findBlock(bus_addr);
                                if(read_update_Block!=NULL)
                                    {
            // If a block is found another copy exists and Flag is posted
                                        main_Bus_Instance->post_Copies_Exist();
            // Updated block states
                                        state read_update_block_state = read_update_Block->getState();
            // From MOD->
                                        if(read_update_block_state==MOD)
                                            {
                                                read_update_Block->setState(OWN);
                                                //cacheArray[i]->num_interventions++;
                                                cacheArray[i]->num_transit_mod_to_own++;
                                                read_update_Block->setFlags(VALID);
                                                main_Bus_Instance->flush_to_Bus();
                                                //cacheArray[i]->num_transfers_cache_to_cache++; //MIGHT BE TEMPORARY
                                                cacheArray[i]->num_flushes++;
                                            }
                                        else if(read_update_block_state==EXC)
                                            {
                                                main_Bus_Instance->Flush_Opt_to_Bus();
                                                cacheArray[i]->num_interventions++;
                                                cacheArray[i]->num_transit_exc_to_shd++;
                                                read_update_Block->setState(SHD);
                                            }
                                        else if(read_update_block_state==OWN)
                                            {
                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;
                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); // End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);
                        // Update new block after flush received
                        // If remote cache Flush
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        // If remote cache FlushOpt
                        if(main_Bus_Instance->isFlushOpt_on_Bus())
                            {
                                main_Bus_Instance->Flush_Opt_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }

                        // Set Tags and Data of block
                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        local_cache_Block->setTag(addr_tag);
                        local_cache_Block->setFlags(VALID);

                        if(main_Bus_Instance->Copies_Exist()==1)
                            {
                                cacheArray[proc_number%num_processors]->num_transit_inv_to_shd++;
                                local_cache_Block->setState(SHD);
                            }
                        else if(main_Bus_Instance->Copies_Exist()==0)
                            {
                                local_cache_Block->setState(EXC);
                                cacheArray[proc_number%num_processors]->num_transit_inv_to_exc++;
                            }
                        main_Bus_Instance->post_no_Copies_Exist(); //Clear copies flag


                    }                                                                           // End read on MISS
                else if(operation=='w')
                    {
                        cacheArray[proc_number%num_processors]->writeMisses++;
                        cacheArray[proc_number%num_processors]->num_transit_inv_to_mod++;

                        assert(main_Bus_Instance->getBusSignal()==clear);
                        main_Bus_Instance->postBusRdX(ul_addr);
                        // Update remote caches
                        for(int i=0; i<num_processors;i++)
                            {
                                if(i== (proc_number%num_processors)) continue;
                                ulong bus_write_addr = main_Bus_Instance->getSignalAddr();
                                cacheBlock* write_update_Block = cacheArray[i]->findBlock(bus_write_addr);
                                if(write_update_Block!=NULL)
                                    {
                                        state write_update_block_state = write_update_Block->getState();
                                        if(write_update_block_state==MOD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;

                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;
                                            }
                                        else if(write_update_block_state==SHD)
                                            {
                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;
                                            }
                                        else if(write_update_block_state==EXC)
                                            {
                                                //FlushOpt
                                                main_Bus_Instance->Flush_Opt_to_Bus();

                                                write_update_Block->setFlags(INVALID);
                                                write_update_Block->setState(INV);
                                                cacheArray[i]->num_invalidations++;
                                            }
                                        else if(write_update_block_state==OWN)
                                            {
                                                main_Bus_Instance->flush_to_Bus();
                                                cacheArray[i]->num_flushes++;

                                                write_update_Block->setState(INV);
                                                write_update_Block->setFlags(INVALID);
                                                cacheArray[i]->num_invalidations++;

                                            }
                                    }
                            }
                        main_Bus_Instance->clearBus(); //End updating remote caches
                        assert(main_Bus_Instance->getBusSignal()==clear);

                        // Update new block after flush received
                        //assert(main_Bus_Instance->isFlush_on_Bus());
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->flush_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        if(main_Bus_Instance->isFlush_on_Bus())
                            {
                                main_Bus_Instance->Flush_Opt_received();
                                cacheArray[proc_number%num_processors]->num_transfers_cache_to_cache++;
                            }
                        ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                        local_cache_Block->setTag(addr_tag);
                        local_cache_Block->setFlags(DIRTY);
                        //cacheArray[proc_number%num_processors]->num_transit_shd_to_mod++; <--?
                        local_cache_Block->setState(MOD);

                    }                                                                       // End write on MISS
            }   // End MISS logic

        else if(local_cache_Block!=NULL)                                                                           // Cache HIT
            {
                cacheArray[proc_number%num_processors]->updateBlock_Least_to_Most_Recently_Used(local_cache_Block);
                if(operation=='w')
                    {
                        if(local_cache_Block->getState()==SHD || local_cache_Block->getState()==OWN)
                            {
                                assert(main_Bus_Instance->getBusSignal()==clear);
                                main_Bus_Instance->postBusUpgr(ul_addr);

                                //Update Remote Caches
                                for(int i=0; i<num_processors; i++)
                                    {
                                        if(i==(proc_number%num_processors)) continue;
                                        ulong hit_bus_write_addr = main_Bus_Instance->getSignalAddr();
                                        cacheBlock* write_update_Block_hit = cacheArray[i]->findBlock(hit_bus_write_addr);
                                        if(write_update_Block_hit!=NULL)
                                            {
                                                state local_state = write_update_Block_hit->getState();
                                                if(local_state==SHD)
                                                    {
                                                        write_update_Block_hit->setFlags(INVALID);
                                                        write_update_Block_hit->setState(INV);
                                                        cacheArray[i]->num_invalidations++;

                                                        cacheArray[i]->num_transit_shd_to_inv++;
                                                    }
                                                else if(local_state==OWN)
                                                    {
                                                        cacheArray[i]->num_invalidations++;
                                                        write_update_Block_hit->setState(INV);
                                                        write_update_Block_hit->setFlags(INVALID);
                                                    }
                                            }
                                    } // End
                                main_Bus_Instance->clearBus();
                                assert(main_Bus_Instance->getBusSignal()==clear);

                                if(local_cache_Block->getState()==SHD) cacheArray[proc_number%num_processors]->num_transit_shd_to_mod++;
                                else if(local_cache_Block->getState()==OWN) cacheArray[proc_number%num_processors]->num_transit_own_to_mod++;
                                ulong addr_tag = cacheArray[proc_number%num_processors]->calcTag(ul_addr);
                                local_cache_Block->setTag(addr_tag);
                                local_cache_Block->setFlags(DIRTY);
                                local_cache_Block->setState(MOD);

                            } // SHD or OWNstate when HIT

                        else if(local_cache_Block->getState()==EXC)
                            {
                                        local_cache_Block->setState(MOD);
                                        cacheArray[proc_number%num_processors]->num_transit_exc_to_mod++;
                                        local_cache_Block->setFlags(DIRTY);
                            }
                        assert(local_cache_Block->getState()!=INV);
                    } // End write on Hit
            }   // End HIT logic
    } // End sim

	// Close input file
	fclose(inputFile);

	//print out all caches' statistics
	for(int i=0; i<num_processors; i++) cacheArray[i]->printStats(num_processors);
    if(!read_from_console)for(int i=0; i<num_processors; i++) cacheArray[i]->printData_for_Excel(num_processors);

	// Deallocate Cache Array
	for(int i=0; i<num_processors; i++) delete cacheArray[i];
	// Deallocate string
	delete outFilename;
	delete main_Bus_Instance;

	return;
}

int main(int argc, char *argv[])
{
	// Sim args
	int cache_size, cache_assoc, blk_size, num_processors, assigned_protocol;
	fname = (char *)malloc(20);

// Only run a simulation with argument variables
if(read_from_console)
{
	// Arg variables
	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }
	cache_size = atoi(argv[1]);
	cache_assoc= atoi(argv[2]);
	blk_size = atoi(argv[3]);
	//1, 2, 4, 8
	num_processors = atoi(argv[4]);
	//0:MSI, 1:MESI, 2:MOESI
	assigned_protocol   = atoi(argv[5]);
 	strcpy(fname, argv[6]);

	if(assigned_protocol == 0)


	simulate_MSI(cache_size, cache_assoc, blk_size, num_processors);
	else if(assigned_protocol == 1) simulate_MESI(cache_size, cache_assoc, blk_size, num_processors);
	else if(assigned_protocol == 2) simulate_MOESI(cache_size, cache_assoc, blk_size, num_processors);
	else
		{
		printf("Invalid Protocol!\n");
		return 0;
		}
} // End if statement

// Run series of simulations
else
{
/*----------------------------------------------------------------
	// Cache size will vary: 256KB, 512KB, 1MB, 2MB
	// Cache associativity will vary: 4-way, 8-way, 16-way
	// Cache block size will vary: 64B, 128B, 256B
------------------------------------------------------------------
*/
strcpy(fname, "../Input/CGad\0");
cache_size = 32678; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);



/*
-----------------Test Cases-------------------------------------
----------------------------------------------------------------
	Cache size will vary: 256KB, 512KB, 1MB, 2MB
	Cache associativity will stay at 8
	Block size will be 64B
----------------------------------------------------------------
*/
	{
cache_size = 256000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 512000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 1000000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 2000000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);



	}

/*
-----------------Test Cases-------------------------------------
----------------------------------------------------------------
	Cache associativity will vary: 4-way, 8-way, 16-way
	Cache size: 1MB
	Block size: 64B
----------------------------------------------------------------
*/
	{
cache_size = 1000000; cache_assoc = 4; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 1000000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 1000000; cache_assoc = 16; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);


	}

/*
-----------------Test Cases-------------------------------------
----------------------------------------------------------------
	Cache block size will vary: 64B, 128B, 256B
	Cache size: 1MB
	Cache Associativity: 8-way
----------------------------------------------------------------
*/
	{
cache_size = 1000000; cache_assoc = 8; blk_size = 64;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 1000000; cache_assoc = 8; blk_size = 128;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);

cache_size = 1000000; cache_assoc = 8; blk_size = 256;
simulate_MSI(cache_size, cache_assoc, blk_size, 16);
simulate_MESI(cache_size, cache_assoc, blk_size, 16);
simulate_MOESI(cache_size, cache_assoc, blk_size, 16);



	}


} // End else statement

	delete fname;
	return 0;

} // End main
