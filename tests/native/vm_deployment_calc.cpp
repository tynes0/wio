#include "vm_deployment_calc.h"

#ifndef WIO_DEPLOY_BIAS
#define WIO_DEPLOY_BIAS 100
#endif

extern "C" WIO_CALC_API int WioDeploymentAdd(int left, int right)
{
    return left + right + WIO_DEPLOY_BIAS;
}
