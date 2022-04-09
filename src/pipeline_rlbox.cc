// #include <vips/vips8>
#include <napi.h>

#include "common.h"
#include "operations.h"
#include "pipeline.h"

class PipelineWorkerRLBox : public Napi::AsyncWorker {
    void Execute();
    void OnOK();
}