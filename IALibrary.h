#pragma once
#define _PT_64_



#ifdef __cplusplus
extern "C" {
#endif

	//#ifdef IALibrary_EXPORTS
#define IALibrary_API __declspec(dllexport)
//#else
//#define IALibrary_API __declspec(dllimport)
//#endif



	IALibrary_API int connect();
	IALibrary_API int acquireIm(unsigned char* bp);
	IALibrary_API int conf(int p, int v);
	IALibrary_API int disconnect();
	IALibrary_API int isDeviceConnected();
	IALibrary_API int SetSamplingRate(int i);
	IALibrary_API int SetDelay(int v);
	IALibrary_API int SetCoupling(int i);
	IALibrary_API int SetNumOfFrames(int n);
	IALibrary_API int SetSamplelength(int i);
	IALibrary_API int SetADCGain(int v);
	IALibrary_API int SetFilterBandwidth(int i);
	IALibrary_API int setTimesArray(double* t, double* t2);
	IALibrary_API int RequestDebug();
	IALibrary_API int AbortDebug();

#ifdef __cplusplus
}
#endif