//
// Parameters Module
//

#include <string.h>
#include <stdbool.h>

#include "../pt/pt.h"

#include "../hal/storage.h"

#include "../modules/link.h"
#include "../modules/module.h"
#include "../modules/control.h"
#include "../modules/parameters.h"

#define PARAMETERS_BANK_ADDRESS 0
#define PARAMETERS_BANK(b) (STORAGE_HAL_PAGE_SIZE * \
                            (1 + (sizeof(struct parameters) + \
                                  STORAGE_HAL_PAGE_SIZE - 1) / STORAGE_HAL_PAGE_SIZE) * (b))

// Public Variables
struct parameters parameters;

// Private Variables
static struct pt parametersThread;
static struct parameters tmpParameters[2];
static unsigned int parametersAddress;

static bool checkParameters(struct parameters* params)
{
  // TODO: check ranges of everything in the parameters
  return true;
}

static bool differentLinkAndParameters(void)
{
  return ((comm.startVentilation != parameters.startVentilation) ||
          (comm.ventilationMode != parameters.ventilationMode) ||
          (comm.volumeRequested != parameters.volumeRequested) ||
          (comm.respirationRateRequested != parameters.respirationRateRequested) ||
          (comm.ieRatioRequested != parameters.ieRatioRequested));
}

static PT_THREAD(parametersThreadMain(struct pt* pt))
{
  PT_BEGIN(pt);
  
  // Read out the current parameter settings
  PT_WAIT_UNTIL(pt, storageHalRead(PARAMETERS_BANK_ADDRESS,
                                   &parametersAddress,
                                   sizeof(parametersAddress)) != HAL_IN_PROGRESS);
  PT_WAIT_UNTIL(pt, storageHalRead(PARAMETERS_BANK(parametersAddress),
                                   &tmpParameters[0],
                                   sizeof(tmpParameters[0])) != HAL_IN_PROGRESS);
  
  if (!checkParameters(&tmpParameters[0])) {
    // TODO: Handle error reading back in parameters (ie, parameters out of range)
    PT_EXIT(pt);
  }

  // Copy the parameters from storage into the publicly available buffer
  parameters = tmpParameters[0];
 
  while (1) {
    // TODO: Determine all the conditions that update the parameters; placeholder for now
    PT_WAIT_UNTIL(pt, differentLinkAndParameters());
    
    tmpParameters[0] = parameters;
    if (differentLinkAndParameters()) {
      // Copy over the paramaters from the comm module
      tmpParameters[0].startVentilation = comm.startVentilation;
      tmpParameters[0].ventilationMode = comm.ventilationMode;
      tmpParameters[0].volumeRequested = comm.volumeRequested;
      tmpParameters[0].respirationRateRequested = comm.respirationRateRequested;
      tmpParameters[0].ieRatioRequested = comm.ieRatioRequested;
    }
    
    if (!checkParameters(&tmpParameters[0])) {
      // TODO: Handle bad parameters in update
    } else {
      // Write the new parameters into the other bank and then read them back,
      // confirming the new parameters are valid
      PT_WAIT_UNTIL(pt, storageHalWrite(PARAMETERS_BANK(!parametersAddress),
                                        &tmpParameters[0],
                                        sizeof(tmpParameters[0])) != HAL_IN_PROGRESS);
      PT_WAIT_UNTIL(pt, storageHalRead(PARAMETERS_BANK(!parametersAddress),
                                       &tmpParameters[1],
                                       sizeof(tmpParameters[1])) != HAL_IN_PROGRESS);
      if (!memcmp(&tmpParameters[0], &tmpParameters[1], sizeof(tmpParameters[0]))) {
        // TODO: Error on parameter mismatch after read-back
      } else {
        // Write back new parameters address to get the data from the new bank next time
        parametersAddress = !parametersAddress;
        PT_WAIT_UNTIL(pt, storageHalWrite(PARAMETERS_BANK_ADDRESS,
                                          &parametersAddress,
                                          sizeof(parametersAddress)) != HAL_IN_PROGRESS);
        // Finally, update current operating parameters
        parameters = tmpParameters[0];
      }
    }
  }
  
  // Should never reach here
  PT_END(pt);
}

int parametersModuleInit(void)
{
  PT_INIT(&parametersThread);
  
  return MODULE_OK;
}

int parametersModuleRun(void)
{
  return (PT_SCHEDULE(parametersThreadMain(&parametersThread))) ? MODULE_OK : MODULE_FAIL;
}