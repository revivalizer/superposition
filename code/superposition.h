/* 
superposition
public domain audio library
by Ralph Brorsen aka revivalizer

No warranty implied; use at your own risk.

REQUIREMENTS
C99
	include: stdint.h (intptr types)




This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*/

#ifdef SUPERPOSITION_HEADER_GUARD
#define SUPERPOSITION_HEADER_GUARD




#endif // SUPERPOSITION_HEADER_GUARD

#ifdef SUPERPOSITION_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4820)

// TODO(revivalizer): STUFF THAT NEEDS TO BE BACKEND/OS DEPENDENT
#define SUPERPOSITION_ASSERT(Cond) assert(Cond)
typedef HWND sp_window;
#define SP_FSIN(x) ((float)sin((float)(x)))


typedef struct {
	void* Base;
	uintptr_t Size;
	uintptr_t Used;
} sp__memory_arena;

void sp__init_memory_arena(sp__memory_arena* Arena, void* Mem, uintptr_t Size)
{
	// TODO(revivalizer): Make sure Mem is 16-byte aligned
	Arena->Base = Mem;
	Arena->Size = Size;
	Arena->Used = 0;
}

#define sp__push_struct(Arena, Type) (Type*)sp__push_struct_(Arena, sizeof(Type))
#define sp__push_array(Arena, Type, Count) (Type*)sp__push_struct_(Arena, sizeof(Type)*Count)

void* sp__push_struct_(sp__memory_arena* Arena, int Size)
{
	SUPERPOSITION_ASSERT(Arena->Used + Size <= Arena->Size);
	void* Result = (void*)((uintptr_t)Arena->Base + Arena->Used);

	Size = (Size + 15) & ~15; // Always allocate 16-byte aligned
	Arena->Used += Size;
	return(Result);
}



enum {
	sp_directsound_error,
	sp_unpack_unrecognized_type,
	sp_unpack_unsupported_wav_format,
};

typedef struct {
	int HasError;
	int DUMMY;
	const char* ErrorMessage;
	int ErrorType;
	union {
		HRESULT HResult;
	} Error;

} sp_error;

void sp__reset_error(sp_error* Error)
{
	Error->HasError = 0;
	Error->ErrorMessage = 0;
}




typedef struct {
	LPDIRECTSOUND		DirectSound;
	LPDIRECTSOUNDBUFFER	PrimaryBuffer;
	LPDIRECTSOUNDBUFFER	SecondaryBuffer;
	DWORD               WritePos;
} sp__directsound;

typedef struct {
	int NumSamples;
	float* Samples;
} sp_sample;

typedef struct {
	sp__memory_arena CoreArena;
	sp_error LastError;
	sp_window Window;
	sp_sample* CurrentSample;
	int CurrentSampleOffset;
	union {
		sp__directsound DirectSound;
	} Backend;
} sp_system;




sp_error* sp_get_last_error(sp_system* System) {
	return &System->LastError;
}

void sp__directsound_set_error(sp_system* System, const char* ErrorMessage, HRESULT Result)
{
	System->LastError.HasError = 1;
	System->LastError.ErrorMessage = ErrorMessage;
	System->LastError.ErrorType = sp_directsound_error;
	System->LastError.Error.HResult = Result;
}

#define	SP__DIRECTSOUND_REPLAY_RATE				44100
#define	SP__DIRECTSOUND_REPLAY_DEPTH			16
#define	SP__DIRECTSOUND_REPLAY_CHANNELS			2
#define	SP__DIRECTSOUND_REPLAY_SAMPLESIZE		(SP__DIRECTSOUND_REPLAY_DEPTH/8)
#define	SP__DIRECTSOUND_REPLAY_BUFFERLEN		(4*32*32*1024)
#define	SP__DIRECTSOUND_REPLAY_BUFFERLEN		(4*32*32*1024)

int sp__directsound_open(sp_system* System) {
	// TODO(revivalizer): Assert correct backend?

	DSBUFFERDESC BufferDesc = {
		sizeof(DSBUFFERDESC), // size
		DSBCAPS_PRIMARYBUFFER | DSBCAPS_STICKYFOCUS, // flags
		0, // buffer bytes
		0, // reserved
		0, // format
		0, // guid
	};

	WAVEFORMATEX BufferFormat =
	{
		WAVE_FORMAT_PCM, // format
		SP__DIRECTSOUND_REPLAY_CHANNELS, // channels
		SP__DIRECTSOUND_REPLAY_RATE, // sample rate
		SP__DIRECTSOUND_REPLAY_RATE * SP__DIRECTSOUND_REPLAY_CHANNELS * SP__DIRECTSOUND_REPLAY_SAMPLESIZE, // bytes per second
		SP__DIRECTSOUND_REPLAY_CHANNELS * SP__DIRECTSOUND_REPLAY_SAMPLESIZE, // block align
		SP__DIRECTSOUND_REPLAY_DEPTH, // sample depth
		0, // extra data
	};

	HRESULT Result;

	sp__directsound* Backend = &System->Backend.DirectSound;

	Result = DirectSoundCreate(0, &Backend->DirectSound, 0);
	if (Result != DS_OK)
	{
		sp__directsound_set_error(System, "Superposition DirectSound Backend: Could not create DirectSound.", Result);
		return 0;
	}

	IDirectSound_SetCooperativeLevel(Backend->DirectSound, System->Window, DSSCL_EXCLUSIVE | DSSCL_PRIORITY);
	if (Result != DS_OK)
	{
		sp__directsound_set_error(System, "Superposition DirectSound Backend: Could not set cooperative level.", Result);
		return 0;
	}

	Result = IDirectSound_CreateSoundBuffer(Backend->DirectSound, &BufferDesc, &Backend->PrimaryBuffer, NULL);
	if (Result != DS_OK)
	{
		sp__directsound_set_error(System, "Superposition DirectSound Backend: Could not create primary buffer.", Result);
		return 0;
	}

	Result = IDirectSoundBuffer_SetFormat(Backend->PrimaryBuffer, &BufferFormat);
	if (Result != DS_OK)
	{
		sp__directsound_set_error(System, "Superposition DirectSound Backend: Could not set format of primary buffer.", Result);
		return 0;
	}

	BufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;
	BufferDesc.dwBufferBytes = SP__DIRECTSOUND_REPLAY_BUFFERLEN;
	BufferDesc.lpwfxFormat = &BufferFormat;
	Result = IDirectSound_CreateSoundBuffer(Backend->DirectSound, &BufferDesc, &Backend->SecondaryBuffer, 0);

	if (Result != DS_OK)
	{
		sp__directsound_set_error(System, "Superposition DirectSound Backend: Could not set create secondary buffer.", Result);
		return 0;
	}

	IDirectSoundBuffer_Play(Backend->SecondaryBuffer, 0, 0, DSBPLAY_LOOPING);

	return 1;
}

int sp__directsound_close(sp_system* System) {
	// TODO(revivalizer): Assert correct backend
	sp__directsound* Backend = &System->Backend.DirectSound;

	if (Backend->SecondaryBuffer) IDirectSoundBuffer_Release(Backend->SecondaryBuffer);
	if (Backend->PrimaryBuffer)   IDirectSoundBuffer_Release(Backend->PrimaryBuffer);
	if (Backend->DirectSound)     IDirectSound_Release(Backend->DirectSound);

	return 1;
}

const float PI2f = (float)(M_PI * 2.0);

int sp__directsound_update(sp_system* System, float latency) {
	static float Phase = 0.f;
	static float Phase2 = 0.f;

	sp__directsound* Backend = &System->Backend.DirectSound;

	DWORD ReadPos = 0;
	IDirectSoundBuffer_GetCurrentPosition(Backend->SecondaryBuffer, &ReadPos, NULL);

	DWORD DesiredLatencyInSamples = (DWORD)(latency*(float)(SP__DIRECTSOUND_REPLAY_RATE*SP__DIRECTSOUND_REPLAY_CHANNELS*SP__DIRECTSOUND_REPLAY_SAMPLESIZE));
	DWORD DesiredLatencyInBytes = DesiredLatencyInSamples * SP__DIRECTSOUND_REPLAY_CHANNELS * SP__DIRECTSOUND_REPLAY_SAMPLESIZE;

	DWORD CurrentComputedBytes;
	if (ReadPos <= Backend->WritePos)
		CurrentComputedBytes = Backend->WritePos - ReadPos;
	else
		CurrentComputedBytes = (SP__DIRECTSOUND_REPLAY_BUFFERLEN - ReadPos) + Backend->WritePos;

	// TODO(revivalizer): Failure condition if too far apart
	/*int FallenBehind = CurrentComputedBytes > (SP__DIRECTSOUND_REPLAY_BUFFERLEN/2);
	if (FallenBehind)
	{
		 = 
	}*/

	DWORD NecessaryBytes = DesiredLatencyInBytes - CurrentComputedBytes;

	if (NecessaryBytes <= 0)
		return 1;

    LPVOID  BufferPointer1, BufferPointer2;
    DWORD   BufferSize1, BufferSize2;

	// TODO(revivalizer): Error condition
	IDirectSoundBuffer_Lock(Backend->SecondaryBuffer, Backend->WritePos, NecessaryBytes, &BufferPointer1, &BufferSize1, &BufferPointer2, &BufferSize2, 0);

	int16_t* ShortBufferPointer1 = (int16_t*)BufferPointer1;
	int16_t* ShortBufferPointer2 = (int16_t*)BufferPointer2;

	float* CurrentSample = System->CurrentSample ? System->CurrentSample->Samples : 0;

	for (DWORD i=0; i<BufferSize1/(SP__DIRECTSOUND_REPLAY_CHANNELS); i++)
	{
		float v = 0.f;

		if (CurrentSample)
			v += CurrentSample[System->CurrentSampleOffset++];

		int16_t u = (int16_t)(v * 32000.f);

		*ShortBufferPointer1++ = u;
	}

	for (DWORD i=0; i<BufferSize2/(SP__DIRECTSOUND_REPLAY_CHANNELS); i++)
	{
		float v = 0.f;

		if (CurrentSample)
			v += CurrentSample[System->CurrentSampleOffset++];

		int16_t u = (int16_t)(v * 32000.f);

		*ShortBufferPointer2++ = u;
	}

	IDirectSoundBuffer_Unlock(Backend->SecondaryBuffer, BufferPointer1, BufferSize1, BufferPointer2, BufferSize2);

	Backend->WritePos += NecessaryBytes;

	if (Backend->WritePos >= SP__DIRECTSOUND_REPLAY_BUFFERLEN)
		Backend->WritePos -= SP__DIRECTSOUND_REPLAY_BUFFERLEN;

	return 1;
}

sp_system* sp_open(sp_window Window, void* CoreMem, int CoreMemSize)
{
	sp__memory_arena TempCoreArena;
	sp__init_memory_arena(&TempCoreArena, CoreMem, CoreMemSize);

	// Move CoreArena to system allocation and reset temp arena
	sp_system* System = sp__push_struct(&TempCoreArena, sp_system);
	memset(CoreMem, 0, CoreMemSize);
	System->CoreArena = TempCoreArena;
	System->Window = Window;
	sp__init_memory_arena(&TempCoreArena, 0, 0);

	if (!sp__directsound_open(System))
		return 0;

	return System;
}

int sp_update(sp_system* System, float latency) {
	return sp__directsound_update(System, latency);
}

int sp_close(sp_system* System) {
	return sp__directsound_close(System);
}

int sp_play(sp_system* System, sp_sample* Sample) {
	System->CurrentSample = Sample;
	System->CurrentSampleOffset = 0;
	return 1;
}


void sp__wav_set_error(sp_system* System, const char* ErrorMessage, int ErrorType)
{
	System->LastError.HasError = 1;
	System->LastError.ErrorMessage = ErrorMessage;
	System->LastError.ErrorType = ErrorType;
}

typedef struct {
	uint32_t ChunkID;
	uint32_t ChunkSize;
	uint32_t Format;
} sp__wav_header;

typedef struct {
	uint32_t ChunkID;
	uint32_t ChunkSize;
} sp__wav_data_chunk;

typedef struct {
	uint32_t ChunkID;
	uint32_t ChunkSize;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
} sp__wav_format_chunk;

typedef struct {
	sp__wav_header* Header;
	sp__wav_format_chunk* Format;
	sp__wav_data_chunk* Data;
	uintptr_t SampleData;
} sp__parsed_wav;

int sp__wav_parse(uintptr_t Sample, sp__parsed_wav* Wav) {
	memset(Wav, 0, sizeof(*Wav));

	sp__wav_header* Header = (sp__wav_header*)Sample;

	if (Header->ChunkID != 'FFIR')
		return 0;
	if (Header->Format != 'EVAW')
		return 0;

	int Size = Header->ChunkSize;

	uintptr_t FileEnd = Sample + Size + 8;

	Wav->Header = Header;

	uintptr_t CurrentChunk = Sample + 12;

	while (CurrentChunk < FileEnd)
	{
		sp__wav_format_chunk* Chunk = (sp__wav_format_chunk*)CurrentChunk;
		if (Chunk->ChunkID == ' tmf')
		{
			Wav->Format = Chunk;
			break;
		}

		CurrentChunk += 8 + Chunk->ChunkSize;
	}

	if (Wav->Format != 0)
	{
		while (CurrentChunk < FileEnd)
		{
			sp__wav_data_chunk* Chunk = (sp__wav_data_chunk*)CurrentChunk;
			if (Chunk->ChunkID == 'atad')
			{
				Wav->Data = Chunk;
				Wav->SampleData = CurrentChunk + 8;
				return 1;
			}

			CurrentChunk += 8 + Chunk->ChunkSize;
		}
	}

	return 0;
}

int sp__is_wav(void* Sample) {
	sp__wav_header* Wav = (sp__wav_header*)Sample;
	return Wav->ChunkID == 'FFIR' && Wav->Format == 'EVAW';
}

int sp__is_supported_wav_format(sp__parsed_wav* Wav) {
	if (Wav->Format->AudioFormat != WAVE_FORMAT_PCM)
		return 0;
	if (Wav->Format->NumChannels != 1 && Wav->Format->NumChannels != 2)
		return 0;
	if (Wav->Format->SampleRate != 44100)
		return 0;
	if (Wav->Format->BitsPerSample != 24)
		return 0;

	return 1;
}

int sp_unpacked_size(sp_system* System, void* Sample) {
	if (sp__is_wav(Sample))
	{
		sp__parsed_wav Wav;
		if (!sp__wav_parse((uintptr_t)Sample, &Wav) || !sp__is_supported_wav_format(&Wav)) {
			sp__wav_set_error(System, "Superposition Unpack: Unsupported wav file format.", sp_unpack_unsupported_wav_format);
			return 0;
		}

		int HeaderSize = sizeof(sp_sample);
		HeaderSize = (HeaderSize + 15) & (~15);

		int NumSamples = Wav.Data->ChunkSize / Wav.Format->BlockAlign;

		int NumBytes = HeaderSize + NumSamples*2*sizeof(float);
		return NumBytes;
	}
	else
	{
		sp__wav_set_error(System, "Superposition Unpack: Unrecognized file format.", sp_unpack_unrecognized_type);
		return 0;
	}
}

sp_sample* sp_unpack(sp_system* System, void* Sample, void* Dest) {
	sp_sample* UnpackedSample = (sp_sample*)Dest;

	if (sp__is_wav(Sample))
	{
		sp__parsed_wav Wav;
		if (!sp__wav_parse((uintptr_t)Sample, &Wav) || !sp__is_supported_wav_format(&Wav)) {
			sp__wav_set_error(System, "Superposition Unpack: Unsupported wav file format.", sp_unpack_unsupported_wav_format);
			return 0;
		}

		int HeaderSize = sizeof(sp_sample);
		HeaderSize = (HeaderSize + 15) & (~15);

		int NumSamples = Wav.Data->ChunkSize / Wav.Format->BlockAlign;

		UnpackedSample->NumSamples = NumSamples;
		UnpackedSample->Samples = (float*)((uintptr_t)Dest + HeaderSize);

		uint8_t* SourceSamples = (uint8_t*)Wav.SampleData;
		float* DestSamples = UnpackedSample->Samples;

		for (int i=0; i<UnpackedSample->NumSamples*2; i++)
		{
			int Left = (SourceSamples[2] << 16) + (SourceSamples[1] << 8) + SourceSamples[0];

			Left |= (Left & (1 << 23)) ? 0xFF000000 : 0;

			DestSamples[0] = (float)Left / (float)(1 << 23);

			SourceSamples += 3;
			DestSamples += 1;
		}

		return UnpackedSample;
	}
	else
	{
		sp__wav_set_error(System, "Superposition Unpack: Unrecognized file format.", sp_unpack_unrecognized_type);
		return 0;
	}
}




#pragma warning(pop)
#endif // SUPERPOSITION_IMPLEMENTATION
