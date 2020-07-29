#include "MultilevelCacheModule.h"

void MultilevelCacheModule::checkCacheParameters(cache_parameters *parameters, uint i, uint numLvlCache){
    if(i == 0 && parameters[i].replPolicy == ReplacementPolicy::PREDETERMINED) {
        cerr << "First level of Cache must not use Chosen Victim as Replacement Policy " << endl;
        exit(EXIT_FAILURE);
    }
    if(parameters[i].ways == 0 || (((i+1)<numLvlCache) && (parameters[i+1].ways==0))){
        cerr << "Number of ways cannot be equal to 0 " << endl;
        exit(EXIT_FAILURE);
    }
    if(parameters[i].cacheDim < parameters[i].blockDim){
        cerr << "The dimension of the cache must be bigger than the size of the block " << endl;
        exit(EXIT_FAILURE);
    }
    if(parameters[i].blockDim == 0 || (parameters[i].blockDim & (parameters[i].blockDim-1) != 0)){ //blockDim potenza di 2
        cerr << "The dimension of the cache block cannot be equal to zero and must be a power of 2 " << endl;
        exit(EXIT_FAILURE);
    }
    if(parameters[i].cacheDim == 0 || (parameters[i].cacheDim & (parameters[i].cacheDim-1) != 0)){ // cacheDim potenza di 2
        cerr << "The dimension of the cache cannot be equal to zero and must be a power of 2 " << endl;
        exit(EXIT_FAILURE);
    }
    if((parameters[i].cacheDim/parameters[i].blockDim) % parameters[i].ways != 0){ // ogni via deve contenere lo stesso numero di blocchi
        cerr << "The number of block must be divisible by the number of ways " << endl;
        exit(EXIT_FAILURE);
    }
    if(parameters[i].writePolicy == WRITE_BACK){ //write-back non implementata
        cerr << "The write back policy is not supported " << endl;
        exit(EXIT_FAILURE);
    }
    if((i+1) < numLvlCache){ // controllo cache successiva
       if(parameters[i].ways == 1) {   // direct mapped
            // L1 deve avere meno o lo stesso numero di entries di L2
            if(parameters[i+1].replPolicy == ReplacementPolicy::PREDETERMINED || (parameters[i].cacheDim/parameters[i].ways > (parameters[i+1].cacheDim/parameters[i+1].ways))) {
                cerr << "L" << i+1 << " is direct mapped. L" << i+2 << " cannot be chosen victim and the number of entries has to be equal or bigger. " << endl;
                exit(EXIT_FAILURE);
            }
        } else { // memoria non direct mapped
            if(parameters[i+1].replPolicy != ReplacementPolicy::PREDETERMINED || (parameters[i+1].ways < parameters[i].ways) || (parameters[i].cacheDim/parameters[i].ways != (parameters[i+1].cacheDim/parameters[i+1].ways))){
                cerr << "L" << i+2 << " has to be Chosen Victim, its number of ways has to be equal or bigger than L" << i+1 << " and the number of entries has to be equal " << endl;
                exit(EXIT_FAILURE);
            }
        }
        if(parameters[i].blockDim != parameters[i+1].blockDim){ // blocchi sempre di dim uguale
            cerr << "All the caches must have the same block dimension " << endl;
            exit(EXIT_FAILURE);
        }
    }
}

MultilevelCacheModule::MultilevelCacheModule(System &sys, Bus &b, string name, uint numLvlCache, cache_parameters *par, int priority):module(name, priority){ // multilevelCache extends module
	string lvlName = "L";
    bus = &b;

	cache_parameters *parameters;
	if(!par && (numLvlCache != 2 && numLvlCache != 0)){
		cerr << "numLvlCache is not coherent with the cache parameters " <<endl;
		exit(EXIT_FAILURE);
	}
	if(!par && numLvlCache > 0){//istanzia una cache inclusiva a due livelli, write_through, write_allocate
		this->numLvlCache = 2;
		parameters = new cache_parameters[numLvlCache];

		parameters[0].writePolicy=L1_DEFAULT_WRITE_POLICY;
		parameters[0].allocationPolicy=L1_DEFAULT_ALLOC_POLICY;
		parameters[0].replPolicy=L1_DEFAULT_REPL_POLICY;
		parameters[0].ways=L1_DEFAULT_WAYS;
		parameters[0].cacheDim=L1_DEFAULT_CACHE_DIM;
		parameters[0].blockDim=L1_DEFAULT_BLOCK_DIM;

		parameters[1].writePolicy=L2_DEFAULT_WRITE_POLICY;
		parameters[1].allocationPolicy=L2_DEFAULT_ALLOC_POLICY;
		parameters[1].replPolicy=L2_DEFAULT_REPL_POLICY;
		parameters[1].ways=L2_DEFAULT_WAYS;
		parameters[1].cacheDim=L2_DEFAULT_CACHE_DIM;
		parameters[1].blockDim=L2_DEFAULT_BLOCK_DIM;
	}
	else{
		parameters = par;
		this->numLvlCache = numLvlCache;
        prevReadCompleted = false;
	}

    if(this->numLvlCache > 0){
        blockSize = parameters[this->numLvlCache - 1].blockDim;
        dummyBlock = new uint32_t[blockSize/sizeof(uint32_t)];
        for(uint i = 0, j = 1; i < this->numLvlCache; i++,j++){
            checkCacheParameters(parameters, i, this->numLvlCache);
            if(i==0){
                cacheLevels.push_back(new AssociativeCache(sys,lvlName + to_string(j),"UpperLvl","LowerLvl", parameters[i].ways,parameters[i].cacheDim,parameters[i].blockDim, CPU_WORD , parameters[i].writePolicy,parameters[i].allocationPolicy, parameters[i].replPolicy));
            }
			else{
                cacheLevels.push_back(new AssociativeCache(sys,lvlName + to_string(j),"UpperLvl","LowerLvl", parameters[i].ways,parameters[i].cacheDim,parameters[i].blockDim, parameters[i-1].blockDim, parameters[i].writePolicy,parameters[i].allocationPolicy, parameters[i].replPolicy));
			}
			sys.addModule(cacheLevels[i]);
		}
    }
	else {
        blockSize = MEM_WORD;
        dummyBlock = new uint32_t[blockSize/sizeof(uint32_t)];
    }
    delete[] parameters;
}

MultilevelCacheModule::~MultilevelCacheModule(){
    if(numLvlCache > 0){
        while(!cacheLevels.empty()){
            delete(cacheLevels.back());
            cacheLevels.pop_back();
        }
    }
    delete[] (dummyBlock);
}

void MultilevelCacheModule::sendMessage(const char* source,const char* dest,void* magic_str,int delay){
    message* msg = new message();
    msg->valid = 1;
    msg->timestamp = getTime();
    strncpy(msg->source, source, sizeof(msg->source));
    strncpy(msg->dest, dest, sizeof(msg->dest));

    msg->magic_struct = magic_str;

    sendWithDelay(msg, delay);
}

memory_message* MultilevelCacheModule::buildCpuMessage(bool type, uint16_t address, uint16_t *data){
	memory_message *MemToCpu = new memory_message();
	if(!MemToCpu){
		cerr << "Failed to allocate dynamic memory " << endl;
		exit(EXIT_FAILURE);
	}
	MemToCpu->type = type;
	MemToCpu->address = address;
	if(data == NULL){
		cerr << "NULL pointer data error " << endl;
		exit(EXIT_FAILURE);
	}
    if(numLvlCache > 0)
        MemToCpu->data = *data;
    else
        MemToCpu->data = data[(MemToCpu->address % MEM_WORD)/sizeof(uint16_t)];
	return MemToCpu;
}

cache_message* MultilevelCacheModule::cpuToCache(memory_message* cpu_mstr){
    cache_message * CpuToCache_mstr = NULL;
    if(cpu_mstr != NULL) {
        CpuToCache_mstr = new cache_message();
        CpuToCache_mstr->type = cpu_mstr->type;
        CpuToCache_mstr->target.address = cpu_mstr->address;
        CpuToCache_mstr->target.data = new uint16_t(cpu_mstr->data);
    }
    return CpuToCache_mstr;
}

memory_message* MultilevelCacheModule::cacheToCpu(cache_message* cache_mstr){
    memory_message * CacheToCpu_mstr = NULL;
    if(cache_mstr != NULL) {
        CacheToCpu_mstr = new memory_message();
        CacheToCpu_mstr->type = cache_mstr->type;
        CacheToCpu_mstr->address = cache_mstr->target.address;
        CacheToCpu_mstr->data = *cache_mstr->target.data;
    }
    return CacheToCpu_mstr;
}

uint16_t* MultilevelCacheModule::createBlock() {
    uint32_t *data = new uint32_t[blockSize/sizeof(uint32_t)];
    for(int i = 0; i < blockSize/sizeof(uint32_t); i++){
        data[i] = dummyBlock[i];
    }
    return (uint16_t*)data;
}

cache_message* MultilevelCacheModule::buildCacheMessage(bool op, uint16_t block_addr){
    cache_message * memToCache_mstr = new cache_message();
    memToCache_mstr->type = op;
    memToCache_mstr->target.address = block_addr ;
    memToCache_mstr->target.data = createBlock();
    return memToCache_mstr;
}

Bus_status* MultilevelCacheModule::createBusStatus(request_t req, uint16_t addr, uint32_t data ){

    Bus_status* status=new Bus_status();
    status->request=req;
    status->address=addr;
    status->data=data;

    return status;
}

int MultilevelCacheModule::extractLvl(string completeName) {
    completeName.erase(0,1);  // rimozione del carattere L
    int num = stoi(completeName);
}

const char* MultilevelCacheModule::computeNextCache(string completeName) {
    // Li -> Li+1
    int num = extractLvl(completeName);
    num ++;
    completeName = "L" + to_string(num);
    return completeName.c_str();
}

const char* MultilevelCacheModule::computePrevCache(string completeName) {
    // Li -> Li-1
    int num = extractLvl(completeName);
    num --;
    completeName = "L" + to_string(num);
    return completeName.c_str();
}

void MultilevelCacheModule::initializeReadFromMemory(uint16_t wordAddress, uint blockSize){
    dummyBlockAddr = wordAddress;
    currentAddr = dummyBlockAddr & (~(blockSize - 1));
    finalAddr = currentAddr + blockSize;
    index = 0;
}

void MultilevelCacheModule::setReadFromMemory(){
    Bus_status *stat = createBusStatus(READ, currentAddr, 0);
    if ( !bus->set(stat) ){  // bus not free
        sendMessage("selfMsg", "UpperLvl", NULL, DELAY_SELF_MSG);
    }
    else {  // bus free
        sendMessage("UpperLvl", "MEM", NULL, DELAY_CACHE_TO_MEMORY);
    }
    delete stat;
}

void MultilevelCacheModule::getReadFromMemory() {
    Bus_status *busStat = new Bus_status();
    if(!busStat || !bus->get(busStat)) {
        cerr << "Bus is busy on read mode or allocation of dynamic memory failed " << endl;
        exit(EXIT_FAILURE);
    }
    dummyBlock[index] = busStat -> data;
    currentAddr += MEM_WORD;
    index ++;
    delete busStat;
}

void MultilevelCacheModule::setWriteOnMemory(){
	if(currentOperation != 1){
		cerr << "The currentOperation is not set as write " << endl;
		exit(EXIT_FAILURE);
	}
	request_t request = WRITE;
	Bus_status *status = createBusStatus(request, addressToWrite, memWordBuff); //creo il "bus status"
	if(!bus->set(status)){ //il bus è occupato, genero un selfmessage per risvegliarmi e ritentare di occupare il bus
		sendMessage("selfMsg", "UpperLvl", NULL, DELAY_SELF_MSG);
	}
	else{ //il bus è libero e ho fatto l'operazione, posso mandare l'ack a tutti i livelli
	    string name;
        if(numLvlCache > 0){
            name = "L" + to_string(numLvlCache);
        }
        else{
            name = cpuName;
        }
        sendMessage("LowerLvl", name.c_str(), NULL, 0);
        sendMessage("UpperLvl", "MEM", NULL, DELAY_CACHE_TO_MEMORY);
	}
	delete status;
}

void MultilevelCacheModule::writeOnMemoryWord(){
	memWordBuff = *dummyBlock;
	uint16_t *pointer = (uint16_t *)&memWordBuff;
	int index;
	if((addressToWrite % 4) < 2){
		index = 0;
	}
	else{
		index = 1;
	}
	pointer[index] = dataToWrite;
}

void MultilevelCacheModule::handleLastLvl(bool type, uint16_t wordAddress, uint16_t data) {
    if(type == 0) {   // READ
        currentOperation = 0;
        initializeReadFromMemory(wordAddress, blockSize); // Inizializza i campi della classe MultilevelCacheModule
        setReadFromMemory();   // se bus occupato invia selfMesg ad UpperLvl
    }
    else {  // WRITE
        addressToWrite = wordAddress;
        dataToWrite = data;
        currentOperation = 1;
        prevReadCompleted = false;
        initializeReadFromMemory(wordAddress, MEM_WORD); // passa come dimensione del blocco 1 word
        setReadFromMemory();
    }
}

void MultilevelCacheModule::onNotify(message* m) {
    if(strcmp("LowerLvl", m->dest) == 0) { // da sx a dx
        #ifdef PRINT
            cout << "Message received. Source: " << m->source << ", Destination: " << m->dest << endl;
        #endif 
        if(strcmp("FETCH", m->source) == 0 || strcmp("DECODE", m->source) == 0) { // source CPU
            cpuName = m->source;
            if(numLvlCache == 0) {  // 0 livelli di cache -> messaggio per la memoria
				memory_message* mm = (memory_message*)m->magic_struct;
                handleLastLvl(mm->type, mm->address, mm->data);
                delete mm;
            }
            else {  // almeno 1 livello di cache
                void *mgst = cpuToCache((memory_message*)m->magic_struct);
                delete (memory_message*)m->magic_struct;
                sendMessage("UpperLvl", "L1", mgst, DELAY_CPU_TO_CACHE);
            }
        }
        else {    // source è livello di cache
            AssociativeCache* lastLevel = cacheLevels.back();
            if(strncmp(m->source, lastLevel->getName().c_str(),sizeof(m->source)) == 0) {   // messaggio dall' ultimo livello di cache
				cache_message* cm = (cache_message*)m->magic_struct;
                handleLastLvl(cm->type, cm->target.address, *(cm->target.data));
                if(cm->victim.data){
                    delete cm->victim.data;
                }
                if(cm->target.data){
                    delete cm->target.data;
                }
                delete cm;
            }
            else {  // messaggio non proviene dall'ultimo livello di cache
                const char* dest = computeNextCache(m->source); // calcola nome del livello successivo
                sendMessage("UpperLvl", dest, m->magic_struct, DELAY_CACHE_TO_CACHE);
            }
        }
        // invalido il messaggio, è per me
        m->valid = 0;
    }
    if(strcmp("UpperLvl", m->dest) == 0) {   // messaggi che vanno da dx a sx
        #ifdef PRINT
            if(strcmp("selfMsg", m->source)){
                cout << "Message received. Source: " << m->source << ", Destination: " << m->dest << endl;
            }
        #endif 
        if(strcmp("selfMsg", m->source) == 0) {
            if(currentOperation == 1 && prevReadCompleted == true) { // write. Gestisce caso in cui bus è occupato per la write -> richiama setWriteOnMemory()
                setWriteOnMemory();
            }
            else {  // read
                setReadFromMemory();
            }
        }
        else{
            if(strcmp("MEM", m->source) == 0){  // la memoria ha messo il dato da leggere nel bus o ha scritto il dato presente sul bus
                getReadFromMemory();    // legge dal bus una memoryWord e aggiorna currentAddr. Quando fa la get dal bus controlla se torna false. Se false allora ritorna un errore e il programma termina
                //vedo quale operazione è stata avviata
                if(currentOperation == 0){  // read
                    if(currentAddr != finalAddr) {   // caso in cui ci sono ancora word da leggere
                        setReadFromMemory();
                    }
                    if(currentAddr == finalAddr) {   // caso in cui è stato letto l'intero blocco
                        void *mstr = NULL;
                        if (numLvlCache == 0){ // caso in cui non è presente Cache e costruisco un messaggio per invio diretto della word
                            mstr = buildCpuMessage(currentOperation, dummyBlockAddr, (uint16_t*)dummyBlock); // crea la magic struct per la CPU, inizializzato con i campi passati per argomento
                            sendMessage("LowerLvl", cpuName.c_str(), mstr, DELAY_CACHE_TO_CPU);
                        }
                        else{ // costruisce messaggio da dare all'ultimo livello di cache e invia il messaggio
                            mstr = buildCacheMessage(currentOperation, dummyBlockAddr); 
                            AssociativeCache* lastLevel= cacheLevels.back();
                            sendMessage("LowerLvl", lastLevel->getName().c_str(), mstr, 0);
                        }
                    }
                }
                else {  // write
                    prevReadCompleted = true;
                    writeOnMemoryWord();  // scrivere la word nella parte più significativa o meno significativa. Scritta nella variabile memWordBuff;
                    setWriteOnMemory(); // Scrittura in memoria. Genera selfMessage se bus è occupato e ack se libero.
                }
            }
            else{  // messaggio proviene da un livello di cache. Può essere ACK o dati
                if(strcmp("L1", m->source) == 0) {
                    sendMessage("LowerLvl", cpuName.c_str(), cacheToCpu((cache_message*)m->magic_struct), DELAY_CACHE_TO_CPU); //crea un messaggio per la CPU
                    if(m->magic_struct){
                        delete[] ((cache_message*)m->magic_struct)->target.data;
                    }
                    delete (cache_message*)m->magic_struct;
                }
                else {
                    const char* dest = computePrevCache(m->source); // calcola nome del livello precedente
                    sendMessage("LowerLvl", dest, m->magic_struct, DELAY_CACHE_TO_CACHE);
                }
            }
        }
        // invalido il messaggio, è per me
        m->valid = 0;
    }
}
