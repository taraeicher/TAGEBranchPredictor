#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"
#include <bitset>

#define HIST_BUFFER_SIZE 1728
#define U_SIZE 1
#define BASE_PREDICTOR_SIZE 16384
#define USE_ALT_COUNTER_MAX 7
#define TAGE_ENTRY_COUNT_1 5120
#define TAGE_ENTRY_COUNT_2 16384
#define TAGE_ENTRY_COUNT_3 8192
#define TAGE_ENTRY_COUNT_4 2048

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for TAGE.

 private:
 
   // Overhead
 
   bitset<HIST_BUFFER_SIZE> historyBuffer;
   int providerIndex;
   int altProviderIndex;
   UINT32 useAltOnNa;
   UINT32 providerPredIndex;
   bool providerPred;
   bool altProviderPred;

   
   // TAGE tables
   typedef struct tageEntry{
	  UINT32 counter;
	  UINT32 tag;
	  bitset<U_SIZE> useful;
   } TageEntry;
   UINT32 basePredictor[BASE_PREDICTOR_SIZE];
   TageEntry T1[TAGE_ENTRY_COUNT_1];
   TageEntry T2[TAGE_ENTRY_COUNT_2];
   TageEntry T3[TAGE_ENTRY_COUNT_3];
   TageEntry T4[TAGE_ENTRY_COUNT_4];
  
 public:

  // The interface to the four functions below CAN NOT be changed

  PREDICTOR(void);
  bool    GetPrediction(UINT32 PC);  
  void    UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);
  void    TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget);

  // Contestants can define their own functions below
  void GetTagePredictions(UINT32 PC, bool* usefulBitNull);
  void UpdateProviderCounter(UINT32 PC, bool resolveDir);
  void AllocateNewEntries(UINT32 PC);
  void SetU(UINT32 PC, bool truthValue);
  void UpdateHistory(bool resolveDir);
  UINT32 GetTageIndex(UINT32 PC, UINT32 tableNum);
  UINT32 GetTageTag(UINT32 PC, UINT32 tableNum);
};



/***********************************************************/
#endif

