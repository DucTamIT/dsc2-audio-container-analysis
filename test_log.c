#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>

int main() {
    FILE *f = fopen("kernel_fast.cl", "rb");
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc(sz + 1); fread(src, 1, sz, f); src[sz] = 0; fclose(f);

    cl_platform_id p; cl_device_id d; cl_uint np;
    clGetPlatformIDs(1, &p, &np); clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL);
    cl_context ctx = clCreateContext(NULL, 1, &d, NULL, NULL, NULL);
    cl_program prog = clCreateProgramWithSource(ctx, 1, (const char **)&src, NULL, NULL);
    cl_int err = clBuildProgram(prog, 1, &d, "-cl-mad-enable -cl-fast-relaxed-math -cl-nv-verbose -cl-nv-maxrregcount=64", NULL, NULL);
    size_t len;
    clGetProgramBuildInfo(prog, d, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
    char *log = malloc(len + 1);
    clGetProgramBuildInfo(prog, d, CL_PROGRAM_BUILD_LOG, len, log, NULL);
    log[len] = 0;
    printf("BUILD LOG:\n%s\n", log);
    return 0;
}
