#define NUM_THREADS_X       1024
#define NUM_THREADS_Y       1
#define NUM_THREADS_Z       1

RWTexture2D<float4> BufferOut : register(u0);
cbuffer UniformBlock0 : register(b1)
{
	int xoffset;
	int yoffset;
};

//! @fn clear_main
//!
//!
[numthreads(NUM_THREADS_X, NUM_THREADS_Y, NUM_THREADS_Z)]
void clear_main(uint3 gid : SV_GroupID, uint gindex : SV_GroupIndex, uint3 dispatchThreadId : SV_DispatchThreadID)
{
  int realOffset = xoffset;
  int2 coord = int2(realOffset + gindex, yoffset + gid.y);
  BufferOut[coord] = float4(0, 0, 0, 0);
}