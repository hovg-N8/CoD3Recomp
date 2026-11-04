#include "blood.h"
void BloodDiagnosticEye(float* eye){eye[0]=eye[1]=eye[2]=0;}
#include "blood_render.h"
#include "blood_projection.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
using Microsoft::WRL::ComPtr;
static uint32_t color=0xff0000ff;static float z=.5f;static bool show=true;
int Blood::BuildWorld(WorldVertex* out,int){if(!show)return 0;out[0]={-.8f,-.8f,z,color};out[1]={.8f,-.8f,z,color};out[2]={0,.8f,z,color};return 3;}
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
 D3D11_TEXTURE2D_DESC t{};t.Width=t.Height=64;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,depth,staging;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;
 CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&target)));CHECK(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)));
 t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&depth)));CHECK(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)));
 t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=0;t.Usage=D3D11_USAGE_STAGING;t.CPUAccessFlags=D3D11_CPU_ACCESS_READ;CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&staging)));
 ID3D11RenderTargetView* view=rtv.Get();context->OMSetRenderTargets(1,&view,dsv.Get());D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
 BloodRender::Projection p{};for(int i=0;i<4;i++)p.matrix[i*4+i]=1;p.scale[0]=1;p.scale[1]=-1;p.scale[2]=1;p.offset[0]=p.offset[1]=.5f;p.target[0]=p.target[1]=2;p.flags[0]=63;
 auto clear=[&](float d){float black[4]={0,0,0,1};context->ClearRenderTargetView(rtv.Get(),black);context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,d,0);};
 auto pixel=[&](){context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)))return 0u;uint32_t c;memcpy(&c,(char*)m.pData+32*m.RowPitch+32*4,4);context->Unmap(staging.Get(),0);return c;};
 clear(.75f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0); // wall in front
 clear(.25f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff); // visible in front of wall
 color=0xff00ff00;z=.4f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff00); // blood did not write depth
 color=0xffff0000;z=.9f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff0000); // later foreground/viewmodel
 clear(.25f);color=0xff0000ff;z=.5f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));color=0x80ffffff;z=.8f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0x00ffff00)!=0); // later translucent layer attenuates
 clear(.25f);color=0xff0000ff;p.matrix[3]=1.8f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0); // current frame projection moves geometry
 // Reproduce a world point in front of the camera clipped by a short-range matrix.
 p={};p.matrix[0]=p.matrix[5]=1;p.matrix[14]=1;
 p.matrix[10]=.05f/(.05f-.4f);p.matrix[11]=.05f*.4f/(.4f-.05f);
 p.scale[0]=1;p.scale[1]=-1;p.scale[2]=1;p.offset[0]=p.offset[1]=.5f;p.target[0]=p.target[1]=2;p.flags[0]=63;
 z=.5f;color=0xff0000ff;clear(.01f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0);
 CHECK(BloodRender::NearPlane(p.matrix)<1);
 p.matrix[10]=6.f/(6.f-8192.f);p.matrix[11]=6.f*8192.f/(8192.f-6.f);
 // Scale the same scene to game units: triangle at 350 units, six-unit near plane.
 z=350; p.matrix[0]=p.matrix[5]=350;
 CHECK(BloodRender::NearPlane(p.matrix)>=1);clear(.01f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff);
 clear(.25f);show=false;p.matrix[3]=0;CHECK(!BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0);
 printf("PASS %d GPU checks: real shader, reversed-depth occlusion, no depth writes, later foreground/transparency, frame matrix and hidden batch.\n",checks);
}



