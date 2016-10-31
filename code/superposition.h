/* 

superposition - free low pain audio library for games

by Ralph Brorsen aka revivalizer

Twitter: @revivalizer
Blog: revivalizer.dk/blog

No warranty implied; use at your own risk.

REQUIREMENTS
C99
	include: stdint.h (intptr types)




This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
*/


// TODO(revivalizer):
// Stop using interleaved channels?
// Clean up update func abstraction
// Better struct typedef structure
// Correctly end sounds
// Click on loop?

#ifndef SUPERPOSITION_HEADER_GUARD
#define SUPERPOSITION_HEADER_GUARD

#ifndef SUPERPOSITION_MAX_INPUT
#define SUPERPOSITION_MAX_INPUT 128
#endif



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

void sp__reset_memory_arena(sp__memory_arena* Arena)
{
	Arena->Used = 0;
}

#define sp__push_struct(Arena, Type) (Type*)sp__push_struct_(Arena, sizeof(Type))
#define sp__push_array(Arena, Type, Count) (Type*)sp__push_struct_(Arena, sizeof(Type)*Count)
#define sp__push_data(Arena, Size) (void*)sp__push_struct_(Arena, Size)

void* sp__push_struct_(sp__memory_arena* Arena, int Size)
{
	SUPERPOSITION_ASSERT(Arena->Used + Size <= Arena->Size);
	void* Result = (void*)((uintptr_t)Arena->Base + Arena->Used);

	Size = (Size + 15) & ~15; // Always allocate 16-byte aligned
	Arena->Used += Size;
	return(Result);
}

sp__memory_arena* sp__push_subarena(sp__memory_arena* ParentArena, int Size)
{
	sp__memory_arena* SubArena = sp__push_struct(ParentArena, sp__memory_arena);
	void* Mem = sp__push_data(ParentArena, Size);
	sp__init_memory_arena(SubArena, Mem, Size);
	return SubArena;
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

typedef struct sp__node sp_node;
typedef struct sp__buffer sp_buffer;
typedef struct sp__sample sp_sample;

typedef void sp_node_update(sp_node*);

struct sp__buffer {
	int NumFrames;
	float* Data;
};

typedef struct {
	sp_sample* Sample;
	int CurrentFrame;
} sp__state_sample_playback;

struct sp__node {
	sp_buffer* Buffer;
	int        BufferRevision;
	sp_node_update* UpdateFunc;
	void*           State;
	sp_node* Inputs[SUPERPOSITION_MAX_INPUT];
	int NumInputs;
	union {
		sp__state_sample_playback SamplePlayback;
	} State2;
};

typedef struct sp__input sp_input;

struct sp__input_list {
	sp_node* Node;
};


typedef struct {
	int NumTotalNodes;
	int NumFreeNodes;
	sp_node** FreeNodes;
} sp__node_allocator;

sp__node_allocator* sp__node_allocator_init(sp__memory_arena* Arena, int NumNodes) {
	sp__node_allocator* Allocator = sp__push_struct(Arena, sp__node_allocator);

	Allocator->NumTotalNodes = NumNodes;
	Allocator->NumFreeNodes = NumNodes;
	Allocator->FreeNodes = sp__push_array(Arena, sp_node*, NumNodes);

	sp_node* Nodes = sp__push_array(Arena, sp_node, NumNodes);

	for (int i=0; i<NumNodes; i++) {
		Allocator->FreeNodes[i] = &(Nodes[i]);
	}

	return Allocator;
}

sp_node* sp__node_alloc(sp__node_allocator* Allocator) {
	SUPERPOSITION_ASSERT(Allocator->NumFreeNodes > 0);

	return Allocator->FreeNodes[--Allocator->NumFreeNodes];
};

void sp__node_free(sp__node_allocator* Allocator, sp_node* Node) {
	SUPERPOSITION_ASSERT(Allocator->NumFreeNodes < Allocator->NumTotalNodes);

	Allocator->FreeNodes[Allocator->NumFreeNodes++] = Node;
};




typedef struct {
	LPDIRECTSOUND		DirectSound;
	LPDIRECTSOUNDBUFFER	PrimaryBuffer;
	LPDIRECTSOUNDBUFFER	SecondaryBuffer;
	DWORD               WritePos;
} sp__directsound;

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
} sp__sample_wav;


enum {
	sp_sample_type_unknown = 0,
	sp_sample_type_wav,
};

struct sp__sample {
	int SampleType;
	int NumFrames;
	union {
		sp__sample_wav Wav;
	} Sample;
};

typedef struct {
	sp__memory_arena CoreArena;
	sp_error LastError;
	sp_window Window;
	union {
		sp__directsound DirectSound;
	} Backend;
	sp__node_allocator* NodeAllocator;
	sp_node* Root;
	int CurrentRevision;
	sp__memory_arena* TempUpdateArena;
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
#define	SP__DIRECTSOUND_REPLAY_FRAMESIZE		(SP__DIRECTSOUND_REPLAY_SAMPLESIZE * SP__DIRECTSOUND_REPLAY_CHANNELS)
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
		SP__DIRECTSOUND_REPLAY_RATE * SP__DIRECTSOUND_REPLAY_FRAMESIZE, // bytes per second
		SP__DIRECTSOUND_REPLAY_FRAMESIZE, // block align
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

void sp__node_update_recursive(sp_system* System, sp_node* Node, int NumSamples);

int sp__directsound_update(sp_system* System, float latency) {
	sp__directsound* Backend = &System->Backend.DirectSound;

	DWORD ReadPos = 0;
	IDirectSoundBuffer_GetCurrentPosition(Backend->SecondaryBuffer, &ReadPos, NULL);

	// TODO(revivalizer): This is 4 times more latency that we asked for. Need to investigate.
	DWORD DesiredLatencyInFrames = 4*(DWORD)(latency*(float)(SP__DIRECTSOUND_REPLAY_RATE));
	DWORD DesiredLatencyInBytes = DesiredLatencyInFrames * SP__DIRECTSOUND_REPLAY_FRAMESIZE;

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

	sp__reset_memory_arena(System->TempUpdateArena);
	System->CurrentRevision++;
	int RequiredFrames = BufferSize1/(SP__DIRECTSOUND_REPLAY_FRAMESIZE);
	int RequiredSamples = RequiredFrames*SP__DIRECTSOUND_REPLAY_CHANNELS;
	sp__node_update_recursive(System, System->Root, RequiredFrames);

	float* CurrentSample = System->Root->Buffer->Data;

	for (int i=0; i<RequiredSamples; i++)
		*ShortBufferPointer1++ = (int16_t)((*CurrentSample++) * 32000.f); // TODO: MUL

	if (BufferSize2 > 0)
	{
		sp__reset_memory_arena(System->TempUpdateArena);
		System->CurrentRevision++;
		RequiredFrames = BufferSize2 / (SP__DIRECTSOUND_REPLAY_FRAMESIZE);
		RequiredSamples = RequiredFrames*SP__DIRECTSOUND_REPLAY_CHANNELS;
		sp__node_update_recursive(System, System->Root, RequiredFrames);

		CurrentSample = System->Root->Buffer->Data;

		for (int i = 0; i < RequiredSamples; i++)
			*ShortBufferPointer2++ = (int16_t)((*CurrentSample++) * 32000.f); // TODO: MUL
	}

	IDirectSoundBuffer_Unlock(Backend->SecondaryBuffer, BufferPointer1, BufferSize1, BufferPointer2, BufferSize2);

	Backend->WritePos += BufferSize1 + BufferSize2;

	if (Backend->WritePos >= SP__DIRECTSOUND_REPLAY_BUFFERLEN)
		Backend->WritePos -= SP__DIRECTSOUND_REPLAY_BUFFERLEN;

	return 1;
}

void sp__node_update_recursive(sp_system* System, sp_node* Node, int NumFrames)
{
	if (Node->BufferRevision == System->CurrentRevision)
		return;

	for (int i=0; i<Node->NumInputs; i++)
		sp__node_update_recursive(System, Node->Inputs[i], NumFrames);

	Node->Buffer = sp__push_struct(System->TempUpdateArena, sp_buffer);
	Node->Buffer->NumFrames = NumFrames;
	Node->Buffer->Data = sp__push_data(System->TempUpdateArena, sizeof(float)*SP__DIRECTSOUND_REPLAY_CHANNELS*NumFrames);
	Node->BufferRevision = System->CurrentRevision;

	Node->UpdateFunc(Node);
}

void sp_update_sample(sp_node* Node) {
	sp__state_sample_playback* Playback = (sp__state_sample_playback*)Node->State;

	for (int i=0; i<Node->Buffer->NumFrames; i++)
	{
		if (Playback->CurrentFrame >= 0 && Playback->CurrentFrame < Playback->Sample->NumFrames)
		{
			// Stereo 24-bit
			// TODO(revivalizer): Smells on next line
			uint8_t* SourceSamples = (uint8_t*)(Playback->Sample->Sample.Wav.SampleData) + Playback->CurrentFrame*6;
			float* DestSamples = Node->Buffer->Data + i*2;

			for (int i=0; i<2; i++)
			{
				int Value = (SourceSamples[2] << 16) + (SourceSamples[1] << 8) + SourceSamples[0];

				Value |= (Value & (1 << 23)) ? 0xFF000000 : 0;

				*DestSamples++ = (float)Value / (float)(1 << 23);
				SourceSamples += 3;
			}
		}
		else
		{
			Node->Buffer->Data[i*2 + 0] = 0.f;
			Node->Buffer->Data[i*2 + 1] = 0.f;
		}

		Playback->CurrentFrame++;

		if (Playback->CurrentFrame >= Playback->Sample->NumFrames)
			Playback->CurrentFrame = 0;
	}
}

void sp_update_sample_mixer(sp_node* Node) {

	int NumSamples = Node->Buffer->NumFrames*SP__DIRECTSOUND_REPLAY_CHANNELS;
	for (int i=0; i<NumSamples; i++)
	{
		float Out = 0.f;

		for (int j=0; j<Node->NumInputs; j++)
			Out += Node->Inputs[j]->Buffer->Data[i];

		Node->Buffer->Data[i] = Out;
	}
}

sp_node* sp_node_create(sp_system* System, sp_node_update* Update, void* State) {
	sp_node* Node = sp__node_alloc(System->NodeAllocator);
	memset(Node, 0, sizeof(sp_node));
	Node->UpdateFunc = Update;
	Node->State = State;
	return Node;
}

sp_node* sp__node_create_default(sp_system* System, sp_node_update* Update) {
	sp_node* Node = sp_node_create(System, Update, 0);
	Node->State = (void*)&(Node->State2);
	return Node;
}

sp_node* sp_make_node_sample_mixer(sp_system* System) {
	return sp_node_create(System, &sp_update_sample_mixer, 0);
}

sp_node* sp_make_node_sample_playback(sp_system* System, sp_sample* Sample) {
	sp_node* Node = sp__node_create_default(System, &sp_update_sample);
	Node->State2.SamplePlayback.Sample = Sample;
	Node->State2.SamplePlayback.CurrentFrame = 0;
	return Node;
}


sp_system* sp_open(sp_window Window, void* CoreMem, int CoreMemSize)
{
	sp__memory_arena TempCoreArena;
	sp__init_memory_arena(&TempCoreArena, CoreMem, CoreMemSize);

	sp_system* System = sp__push_struct(&TempCoreArena, sp_system);
	memset(CoreMem, 0, CoreMemSize);
	System->CoreArena = TempCoreArena;
	System->Window = Window;
	System->NodeAllocator = sp__node_allocator_init(&System->CoreArena, 4); // TODO(revivalizer): Made up number
	System->Root = sp_make_node_sample_mixer(System);
	System->TempUpdateArena = sp__push_subarena(&System->CoreArena, 4*8092*16);

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
	sp_node* Node = sp_make_node_sample_playback(System, Sample);
	System->Root->Inputs[0] = Node; // TODO: HACK
	System->Root->NumInputs = 1;
	return 1;
}


void sp__wav_set_error(sp_system* System, const char* ErrorMessage, int ErrorType)
{
	System->LastError.HasError = 1;
	System->LastError.ErrorMessage = ErrorMessage;
	System->LastError.ErrorType = ErrorType;
}

int sp__wav_parse(uintptr_t Sample, sp__sample_wav* Wav) {
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

	while (CurrentChunk < FileEnd) {
		sp__wav_format_chunk* Chunk = (sp__wav_format_chunk*)CurrentChunk;
		if (Chunk->ChunkID == ' tmf') {
			Wav->Format = Chunk;
			break;
		}
		CurrentChunk += 8 + Chunk->ChunkSize;
	}

	if (Wav->Format != 0) {
		while (CurrentChunk < FileEnd) {
			sp__wav_data_chunk* Chunk = (sp__wav_data_chunk*)CurrentChunk;
			if (Chunk->ChunkID == 'atad') {
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

int sp__is_supported_wav_format(sp__sample_wav* Wav) {
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

int sp_sample_create(sp_system* System, void* Sample, sp_sample* Dest) {
	if (sp__is_wav(Sample))
	{
		if (!sp__wav_parse((uintptr_t)Sample, &Dest->Sample.Wav) || !sp__is_supported_wav_format(&Dest->Sample.Wav)) {
			sp__wav_set_error(System, "Superposition Unpack: Unsupported wav file format.", sp_unpack_unsupported_wav_format);
			return 0;
		}

		Dest->NumFrames = Dest->Sample.Wav.Data->ChunkSize / Dest->Sample.Wav.Format->BlockAlign;

		return 1;
	}
	else
	{
		sp__wav_set_error(System, "Superposition Unpack: Unrecognized sample file format.", sp_unpack_unrecognized_type);
		return 0;
	}
}




#pragma warning(pop)
#endif // SUPERPOSITION_IMPLEMENTATION
