#include "predictor.h"
#include <cmath>

// size definitions

#define HIST_BUFFER_SIZE 1728
#define HIST_POINTER_SIZE 12
#define T0_COUNTER_MAX 7
#define TI_COUNTER_MAX 7
#define U_SIZE 1
#define BASE_PREDICTOR_SIZE 16384
#define USE_ALT_COUNTER_MAX 7
#define TAGE_INDEX_SIZE_1 12
#define TAGE_INDEX_SIZE_2 13
#define TAGE_INDEX_SIZE_3 12
#define TAGE_INDEX_SIZE_4 10
#define TAGE_ENTRY_COUNT_1 5120
#define TAGE_ENTRY_COUNT_2 16384
#define TAGE_ENTRY_COUNT_3 8192
#define TAGE_ENTRY_COUNT_4 2048
#define TAG_SIZE_1 8
#define TAG_SIZE_2 11
#define TAG_SIZE_3 13
#define TAG_SIZE_4 14
#define HIST_LENGTH_1 8
#define HIST_LENGTH_2 48
#define HIST_LENGTH_3 288
#define HIST_LENGTH_4 1728
#define MAX_ALLOCATIONS 1


/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Total storage budget: 524288 bits
// Total size of base predictor: 48K bits
// Total size of each TAGE table group: (1 + 3 + tag bits) * entries
//		T1: (1 + 3 + 8) * 5K = 48K bits
//		T2: (1 + 3 + 11) * 16K = 240K bits
//		T3: (1 + 3 + 13) * 8K = 136K bits
//		T4: (1 + 3 + 14) * 2K = 36K bits
// Total size of TAGE predictor: 508K = 520192 bits
// History buffer size: 1728 bits
// Total control logic size: providerIndex size +
// 		altProviderIndex size + useAltOnNa size +
//		providerPredIndex size + providerPred size
//		+ altProviderPred size = 2 + 2 + 3 + 13 + 1 + 1 = 22
// Total Size: TAGE predictor size + control logic size +  
// buffer size = 485376 + 22 + 1728 = 521942 bits
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

PREDICTOR::PREDICTOR(void){
	
	// Initialize control logic.

   historyBuffer = bitset<HIST_BUFFER_SIZE>();
   useAltOnNa = 0;
   providerPred = false;
   altProviderPred = false;
   providerPredIndex = 99999;
   
    // Initialize base predictor.
	
	for (UINT32 ind = 0; ind < BASE_PREDICTOR_SIZE; ind++){
	  basePredictor[ind] = T0_COUNTER_MAX / 2; 
    }
	
	// Initialize TAGE predictors.
	for (UINT32 ind = 0; ind < TAGE_ENTRY_COUNT_1; ind++){
	  T1[ind].counter = TI_COUNTER_MAX / 2; 
	  T1[ind].tag = pow(TAG_SIZE_1, 2) - 1; 
    }
	for (UINT32 ind = 0; ind < TAGE_ENTRY_COUNT_2; ind++){
	  T2[ind].counter = TI_COUNTER_MAX / 2; 
	  T2[ind].tag = pow(TAG_SIZE_2, 2) - 1; 
    }
	for (UINT32 ind = 0; ind < TAGE_ENTRY_COUNT_3; ind++){
	  T3[ind].counter = TI_COUNTER_MAX / 2; 
	  T3[ind].tag = pow(TAG_SIZE_3, 2) - 1; 
    }
	for (UINT32 ind = 0; ind < TAGE_ENTRY_COUNT_4; ind++){
	  T4[ind].counter = TI_COUNTER_MAX / 2; 
	  T4[ind].tag = pow(TAG_SIZE_4, 2) - 1; 
    }
	
   providerIndex = -1, altProviderIndex = -1;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){
	
	// Fetch the alternate and provider predictions from TAGE.
	// If the provider's prediction is unreliable, use the alt prediction.
	
	bool usefulBitNull = false;
	GetTagePredictions(PC, &usefulBitNull);
	if (usefulBitNull && useAltOnNa > USE_ALT_COUNTER_MAX / 2){
		return altProviderPred;
	}
	else{
		return providerPred;
	}
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

	 // Update the counter based on the branch outcome.
	 
	 UpdateProviderCounter(PC, resolveDir);
	 
	 // In the case of a misprediction, allocate new entries on components with more history.
	 
	 if(resolveDir != predDir){
		AllocateNewEntries(PC);
	 }
	 
	 // Change the useful bit if the alt and provider predictions
	 // are different. Set based on the provider prediction accuracy.
	 // Update whether to use the alternate when the useful bit based on whether
	 // the case has occurred where the alternate prediction is correct and the
	 // provider prediction is not.
	 
	 if(altProviderPred != providerPred){
		if(altProviderPred == resolveDir){
			SetU(PC, false);
			if(useAltOnNa < USE_ALT_COUNTER_MAX){
				 useAltOnNa++;
			}
		}
		else{
			SetU(PC, true);
			 if(useAltOnNa > 0){
				 useAltOnNa--;
			 }
		 }
	 }
	 
	 // Update history.
	 
	 UpdateHistory(resolveDir);
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void PREDICTOR::GetTagePredictions(UINT32 PC, bool* usefulBitNull){

	// Find the provider and alternate provider predictions.
	// The provider is the hitting component with the
	// longest history, and the alternate provider is the
	// hitting component with the second longest history.

	UINT32 counterVal = 0;
	providerIndex = -1;
	altProviderIndex = -1;
	providerPredIndex = 999999;
	
	UINT32 t4PredictorIndex = GetTageIndex(PC, 4);
	UINT32 tag4 = GetTageTag(PC, 4);
	if(T4[t4PredictorIndex].tag == tag4){
		counterVal = T4[t4PredictorIndex].counter;
		if(providerIndex == -1){
			providerIndex = 4;
			providerPredIndex = t4PredictorIndex;
			if (counterVal > (TI_COUNTER_MAX / 2)){
				providerPred = true;
			}
			else{
				providerPred = false;
			}
			*usefulBitNull = T4[t4PredictorIndex].useful[0]==false;
		}
	}
	
	if(providerIndex == -1 || altProviderIndex == -1){
		UINT32 t3PredictorIndex = GetTageIndex(PC, 3);
		UINT32 tag3 = GetTageTag(PC, 3);
		if(T3[t3PredictorIndex].tag == tag3){
			counterVal = T3[t3PredictorIndex].counter;
			if(providerIndex == -1){
				providerIndex = 3;
				providerPredIndex = t3PredictorIndex;
				if (counterVal > (TI_COUNTER_MAX / 2)){
					providerPred = true;
				}
				else{
					providerPred = false;
				}
				*usefulBitNull = T3[t3PredictorIndex].useful[0]==false;
			}
			else{
				altProviderIndex = 3;
				if (counterVal >= 4){
					altProviderPred = true;
				}
				else{
					altProviderPred = false;
				}
			}
		}
	}
	
	if(providerIndex == -1 || altProviderIndex == -1){
		UINT32 t2PredictorIndex = GetTageIndex(PC, 2);
		UINT32 tag2 = GetTageTag(PC, 2);
		if(T2[t2PredictorIndex].tag == tag2){
			counterVal = T2[t2PredictorIndex].counter;	
			if(providerIndex == -1){
				providerIndex = 2;
				providerPredIndex = t2PredictorIndex;
				if (counterVal > (TI_COUNTER_MAX / 2)){
					providerPred = true;
				}
				else{
					providerPred = false;
				}
				*usefulBitNull = T2[t2PredictorIndex].useful[0]==false;
			}
			else{
				altProviderIndex = 2;
				if (counterVal >= 4){
					altProviderPred = true;
				}
				else{
					altProviderPred = false;
				}
			}
		}
	}
	
	if(providerIndex == -1 || altProviderIndex == -1){
		UINT32 t1PredictorIndex = GetTageIndex(PC, 1);
		UINT32 tag1 = GetTageTag(PC, 1);
		if(T1[t1PredictorIndex].tag == tag1){
			counterVal = T1[t1PredictorIndex].counter;
			if(providerIndex == -1){
				providerIndex = 1;
				providerPredIndex = t1PredictorIndex;
				if (counterVal  > (TI_COUNTER_MAX / 2)){
					providerPred = true;
				}
				else{
					providerPred = false;
				}
				*usefulBitNull = T1[t1PredictorIndex].useful[0]==false;
			}
			else{
				altProviderIndex = 1;
				if (counterVal >= 4){
					altProviderPred = true;
				}
				else{
					altProviderPred = false;
				}
			}
		}
	}
	
	if(providerIndex == -1 || altProviderIndex == -1){
		UINT32 basePredictorIndex = PC % BASE_PREDICTOR_SIZE;
		counterVal = basePredictor[basePredictorIndex];
		if(providerIndex == -1){
			providerIndex = 0;
			providerPredIndex = basePredictorIndex;
			if (counterVal  > (T0_COUNTER_MAX / 2)){
				providerPred = true;
			}
			else{
				providerPred = false;
			}
		}
		if (altProviderIndex == -1){
			altProviderIndex = 0;
			if (counterVal > (T0_COUNTER_MAX / 2)){
				altProviderPred = true;
			}
			else{
				altProviderPred = false;
			}
		}
	}
}

void PREDICTOR::UpdateProviderCounter(UINT32 PC, bool resolveDir){	
	
	// Get the counter to update by hashing to the predictor index.
	// If the branch was not taken, decrement the counter. Otherwise, increment it.
	
	switch(providerIndex){
		case 4:{
			if (resolveDir == false){
				if(T4[providerPredIndex].counter > 0){
					T4[providerPredIndex].counter--;
				}
			}
			else{
				if(T4[providerPredIndex].counter < TI_COUNTER_MAX){
					T4[providerPredIndex].counter++;
				}
			}
			break;
		}
		case 3:{
			if (resolveDir == false){
				if(T3[providerPredIndex].counter > 0){
					T3[providerPredIndex].counter--;
				}
			}
			else{
				if(T3[providerPredIndex].counter < TI_COUNTER_MAX){
					T3[providerPredIndex].counter++;
				}
			}
			break;
		}
		case 2:{
			if (resolveDir == false){
				if(T2[providerPredIndex].counter > 0){
					T2[providerPredIndex].counter--;
				}
			}
			else{
				if(T2[providerPredIndex].counter < TI_COUNTER_MAX){
					T2[providerPredIndex].counter++;
				}
			}
			break;
		}
		case 1:{
			if (resolveDir == false){
				if(T1[providerPredIndex].counter > 0){
					T1[providerPredIndex].counter--;
				}
			}
			else{
				if(T1[providerPredIndex].counter < TI_COUNTER_MAX){
					T1[providerPredIndex].counter++;
				}
			}
			break;
		}
		default:{
			if (resolveDir == false){
				if(basePredictor[providerPredIndex] > 0){
					basePredictor[providerPredIndex]--;
				}
			}
			else{
				if(basePredictor[providerPredIndex] < T0_COUNTER_MAX){
					basePredictor[providerPredIndex]++;
				}
			}
			break;
		}
	}
}
	
void PREDICTOR::AllocateNewEntries(UINT32 PC){

	// Update the history.
	
	// Allocate up to four new entries on tables with
	// higher history tags.
	// For each entry, use the approapriate history length as the tag,
	// set the counter to weak, and set the useful bit to null.
	
	UINT32 allocationCount = 0;
	UINT32 predictorIndex;
	TageEntry newEntries[MAX_ALLOCATIONS];
	for(UINT32 i = 0; i <= MAX_ALLOCATIONS - 1; i++){
		newEntries[i].counter = TI_COUNTER_MAX / 2;
		newEntries[i].useful = bitset<U_SIZE>(0);
	}
	if (providerIndex < 1 && allocationCount < MAX_ALLOCATIONS){
		predictorIndex = GetTageIndex(PC, 1);
		if(T1[predictorIndex].useful[0] == false){
			newEntries[allocationCount].tag = GetTageTag(PC, 1);
			T1[predictorIndex] = newEntries[allocationCount];
			allocationCount++;
		}
		else{
			T1[predictorIndex].useful[0] = false;
		}
	}
	if (providerIndex < 2 && allocationCount < MAX_ALLOCATIONS){
		predictorIndex = GetTageIndex(PC, 2);
		if(T2[predictorIndex].useful[0] == false){
			newEntries[allocationCount].tag = GetTageTag(PC, 2);
			T2[predictorIndex] = newEntries[allocationCount];
			allocationCount++;
		}
		else{
			T2[predictorIndex].useful[0] = false;
		}
	}
	if (providerIndex < 3 && allocationCount < MAX_ALLOCATIONS){
		predictorIndex = GetTageIndex(PC, 3);
		if(T3[predictorIndex].useful[0] == false){
			newEntries[allocationCount].tag = GetTageTag(PC, 3);
			T3[predictorIndex] = newEntries[allocationCount];
			allocationCount++;
		}
		else{
			T3[predictorIndex].useful[0] = false;
		}
	}
	if (providerIndex < 4){
		predictorIndex = GetTageIndex(PC, 4);
		if(T4[predictorIndex].useful[0] == false){
			newEntries[allocationCount].tag = GetTageTag(PC, 4);
			T4[predictorIndex] = newEntries[allocationCount];
			allocationCount++;
		}
		else{
			T4[predictorIndex].useful[0] = false;
		}
	}	
}

void PREDICTOR::SetU(UINT32 PC, bool truthValue){

	// Set the useful bit on the provider index.
	switch(providerIndex){
		case 4:
			T4[providerPredIndex].useful[0] = truthValue;
			break;
		case 3:
			T3[providerPredIndex].useful[0] = truthValue;
			break;
		case 2:
			T2[providerPredIndex].useful[0] = truthValue;
			break;
		case 1:
			T1[providerPredIndex].useful[0] = truthValue;
			break;
		default:
			break;
	}
}

void PREDICTOR::UpdateHistory(bool resolveDir){
	 
	 // Update history bit.
	 
	 historyBuffer <<= 1;
	 historyBuffer[0] = resolveDir;
}

UINT32 PREDICTOR::GetTageIndex(UINT32 PC, UINT32 tableNum){

	// XOR sub-bitsets of the global history with the PC and each 
	// other to produce an index.
	
	switch(tableNum){
		case 1:{
			bitset<TAGE_INDEX_SIZE_1> subHistory = bitset<TAGE_INDEX_SIZE_1>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_1 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAGE_INDEX_SIZE_1 - 1];
				bool midVal = subHistory[(TAGE_INDEX_SIZE_1 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAGE_INDEX_SIZE_1 / 2] = midVal ^ historyBuffer[lg];
				
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAGE_INDEX_SIZE_1)));
		}
		case 2:{
			bitset<TAGE_INDEX_SIZE_2> subHistory = bitset<TAGE_INDEX_SIZE_2>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_2 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAGE_INDEX_SIZE_2 - 1];
				bool midVal = subHistory[(TAGE_INDEX_SIZE_2 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAGE_INDEX_SIZE_2 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAGE_INDEX_SIZE_2)));
		}
		case 3:{
			bitset<TAGE_INDEX_SIZE_3> subHistory = bitset<TAGE_INDEX_SIZE_3>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_3 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAGE_INDEX_SIZE_3 - 1];
				bool midVal = subHistory[(TAGE_INDEX_SIZE_3 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAGE_INDEX_SIZE_3 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAGE_INDEX_SIZE_3)));
		}
		case 4:{
			bitset<TAGE_INDEX_SIZE_4> subHistory = bitset<TAGE_INDEX_SIZE_4>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_4 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAGE_INDEX_SIZE_4 - 1];
				bool midVal = subHistory[(TAGE_INDEX_SIZE_4 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAGE_INDEX_SIZE_4 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAGE_INDEX_SIZE_4)));
		}
	}
	return 999999;
}
UINT32 PREDICTOR::GetTageTag(UINT32 PC, UINT32 tableNum){
	
	// XOR sub-bitsets of the global history with the PC and each 
	// other to produce an index.
	
	switch(tableNum){
		case 1:{
			bitset<TAG_SIZE_1> subHistory = bitset<TAG_SIZE_1>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_1 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAG_SIZE_1 - 1];
				bool midVal = subHistory[(TAG_SIZE_1 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAG_SIZE_1 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAG_SIZE_1)));
		}
		case 2:{
			bitset<TAG_SIZE_2> subHistory = bitset<TAG_SIZE_2>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_2 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAG_SIZE_2 - 1];
				bool midVal = subHistory[(TAG_SIZE_2 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAG_SIZE_2 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAG_SIZE_2)));
		}
		case 3:{
			bitset<TAG_SIZE_3> subHistory = bitset<TAG_SIZE_3>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_3 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAG_SIZE_3 - 1];
				bool midVal = subHistory[(TAG_SIZE_3 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAG_SIZE_3 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAG_SIZE_3)));
		}
		case 4:{
			bitset<TAG_SIZE_4> subHistory = bitset<TAG_SIZE_4>();
			UINT32 sm, lg;
			
			// Fold history by tag size.

			for(sm = 0, lg = HIST_LENGTH_4 - 1; sm <= lg; sm++, lg--){
				bool lastVal = subHistory[TAG_SIZE_4 - 1];
				bool midVal = subHistory[(TAG_SIZE_4 / 2) - 1];
				subHistory <<= 1;
				subHistory[0] = lastVal ^ historyBuffer[sm];
				subHistory[TAG_SIZE_4 / 2] = midVal ^ historyBuffer[lg];
			}
			
			// Perform XOR with PC.
			
			return (UINT32)subHistory.to_ulong() ^ (PC % ((UINT32)pow(2, TAG_SIZE_4)));
		}
	}
	return 999999;
}