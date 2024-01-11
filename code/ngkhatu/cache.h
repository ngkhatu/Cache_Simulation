/*******************************************************
                          cache.h
              Amro Awad & Yan Solihin
                           2013
                {ajawad,solihin}@ece.ncsu.edu
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <cmath>
#include <iostream>

// Define unsigned types
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;
// Cache Protocols
typedef enum {MSI, MESI, MOESI} protocol;
// Cache States
typedef enum {MOD, SHD, INV, EXC, OWN} state;
typedef enum {clear, BusUpgr, BusRd, BusRdX} signal;

enum{INVALID = 0, VALID, DIRTY};

class cacheBlock
{
protected:
   ulong tag;
   ulong seq;
   ulong Flags; // 0:invalid, 1:valid, 2:dirty

	// Each cache block maintains a state
	state cache_block_state;

public:
   cacheBlock()            { tag = 0; Flags = INVALID; cache_block_state = INV;}
   ulong getTag()         { return tag; }
   state getState()			{ return cache_block_state;}
   ulong getSeq()         { return seq; }
    ulong getFlags() {return Flags;}

    void setFlags(ulong flags) {Flags = flags;}
   void setSeq(ulong Seq)			{ seq = Seq;}
   void setState(state new_state)			{  cache_block_state = new_state;}
   void setTag(ulong a)   { tag = a; }
   void invalidate()      { tag = 0; Flags = INVALID; }
   bool isValid()         { return ((Flags) != INVALID); }
};

class Bus
{
    signal main_Bus;
    bool Flush;
    bool FlushOpt;
    bool Copies_Exist_Flag;

    ulong signal_addr;
    //ulong signal_cache_id;

public:
    Bus(){main_Bus=clear;Flush=0;Copies_Exist_Flag=0;signal_addr=0;}

    // Flush signalling
    bool isFlush_on_Bus(){return Flush;}
    void flush_to_Bus(){Flush=1;}
    void flush_received(){Flush=0;}
    // FlushOpt signalling
    bool isFlushOpt_on_Bus(){return FlushOpt;}
    void Flush_Opt_to_Bus(){FlushOpt=1;}
    void Flush_Opt_received(){FlushOpt=0;}


    // Bus' Main signals
    signal getBusSignal(){return main_Bus;}
    ulong getSignalAddr(){return signal_addr;}
    //ulong getSignalId(){return signal_cache_id;}
    void postBusRd(ulong addr){main_Bus=BusRd; signal_addr=addr;}//(ulong addr, ulong s_id){main_Bus=BusRd; signal_addr=addr; signal_cache_id=s_id;}
    void postBusRdX(ulong addr){main_Bus=BusRdX; signal_addr=addr;}//(ulong addr, ulong s_id){main_Bus=BusRdX; signal_addr=addr;signal_cache_id=s_id;}
    void postBusUpgr(ulong addr){main_Bus=BusUpgr; signal_addr=addr;}
    void clearBus(){main_Bus=clear; signal_addr=0;}


    // To check if copies exist for MESI- EXC state
    bool Copies_Exist(){return Copies_Exist_Flag;}
    void post_Copies_Exist(){Copies_Exist_Flag=1;}
    void post_no_Copies_Exist(){Copies_Exist_Flag=0;}
};


class Cache
{
public:
	ulong reads,readMisses,writes,writeMisses,writeBacks;
	ulong num_transit_inv_to_exc, num_transit_inv_to_shd, num_transit_mod_to_shd;
	ulong num_transit_exc_to_shd, num_transit_shd_to_mod, num_transit_inv_to_mod;
	ulong num_transit_exc_to_mod, num_transit_own_to_mod, num_transit_mod_to_own;
	ulong num_transit_shd_to_inv, num_transfers_cache_to_cache, num_interventions;
	ulong num_invalidations, num_flushes;

protected:
// Coherence counters
	ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;


// Protocol
    protocol cache_protocol;

// Cache ID
	ulong cache_id;

// Pointer to an Array that holds cache row pointers
	cacheBlock **cache_Instance;

// Pointer to the Bus
    Bus* my_Bus_Instance;

// Cache Block Ordering
    //ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
	ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
	ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}

public:
// Sequencing
	ulong currentCycle;
// Constructor, Destructor
	Cache(int,int,int,int, Bus*);
	~Cache() { delete cache_Instance;}
// Manage Cache LRU Replacement Policy
	cacheBlock* findBlockToReplace(ulong addr);
	cacheBlock* fillBlock(ulong addr);
	cacheBlock* findBlock(ulong addr);
	cacheBlock* get_LeastRecentlyUsed_Block(ulong);
// Retrun private counters
    ulong getRM(){return readMisses;}
	ulong getWM(){return writeMisses;}
	ulong getReads(){return reads;}
	ulong getWrites(){return writes;}
	ulong getWB(){return writeBacks;}

    ulong calcTag(ulong);//     { return (addr >> (log2Blk) );}

// Set Sequence of block to 'currentCycle'
    cacheBlock* Access(ulong,uchar);
	void updateBlock_Least_to_Most_Recently_Used(cacheBlock*);
// Print results
	void printStats(int);
	void printData_for_Excel(int);
// Writeback
    void writeBack(ulong) {writeBacks++;}
// MSI, MESI, or MOESI
    void setProtocol(protocol defined_protocol) {cache_protocol = defined_protocol;}
    protocol getProtocol() {return cache_protocol;}

    //void increment_counter(state from, state to);
};

#endif
