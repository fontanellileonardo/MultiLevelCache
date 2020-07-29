#pragma once
#include <string.h>
#include "module.h"
#include "structures.h"
#include "associative_cache.hpp"
#include "cache_message.h"
#include "memory_message.h"
#include "bus.h"
#include "System.h"
#include "cache_parameters.h"
#include "multiLevelCache_define.h"

class MultilevelCacheModule : public module
{
	vector<AssociativeCache*> cacheLevels; //vector istanze Associative Cache

	Bus *bus; // puntatore al Bus

	string cpuName;	// mittente originario

	bool currentOperation;	// 0: read 1:write (Memoria non manda ACK in scrittura)
	
	bool prevReadCompleted;	//	prima di una write c'è sempre una read. Questo parametro indica se la read è stata completata
	uint16_t addressToWrite; // indirizzo a cui è stata comandata la scrittura
	uint16_t dataToWrite;	// dato da scrivere

	uint numLvlCache;	// numero dei livelli di cache
	uint32_t* dummyBlock;
	uint16_t dummyBlockAddr; // indirizzo richiesto dalla CPU o ultimo livello di cache

	uint32_t memWordBuff;	// contiene word (aggiornata) da scrivere in memoria

	uint16_t currentAddr; // indirizzo che scorre, parte dalla prima locazione del blocco
	uint16_t finalAddr;
	uint index;
	uint blockSize = 2;	// dimensione del blocco dell'ultimo livello cache (MemoryWord quando numLevelCache == 0)

    void sendMessage(const char* source, const char* dest, void* magic_str, int delay);
	memory_message* buildCpuMessage(bool type, uint16_t address, uint16_t *data);
	cache_message* cpuToCache(memory_message* cpu_mstr);
	memory_message* cacheToCpu(cache_message* cache_mstr);
	uint16_t* createBlock();
	cache_message* buildCacheMessage(bool op, uint16_t block_addr);
	Bus_status* createBusStatus(request_t req, uint16_t addr, uint32_t data );
	int extractLvl(string completeName);
	const char* computeNextCache(string completeName);
	const char* computePrevCache(string completeName);
	void initializeReadFromMemory(uint16_t wordAddress, uint blockSize);
	void setReadFromMemory();
	void getReadFromMemory();
	void checkCacheParameters(cache_parameters *parameters, uint i, uint numLvlCache);
	void setWriteOnMemory();
	void writeOnMemoryWord();
	void handleLastLvl(bool type, uint16_t wordAddress, uint16_t data);

public:
	MultilevelCacheModule(System &sys, Bus &b, string name, uint numLvlCache = 2, cache_parameters *par = NULL, int priority = 0);
	~MultilevelCacheModule();

	void onNotify(message* m);
};
