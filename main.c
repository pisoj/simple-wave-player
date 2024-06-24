#include <stdio.h>
#include <string.h>
#include <windows.h>

#define sizeof_member(type, member) (sizeof( ((type *)0)->member ))
#define streq(s1, s2) (strcmp(s1, s2) == 0)

typedef struct PlayerState {
	int bufferSize;
	FILE *file;
	WAVEHDR *headerToUnprepare;
	WAVEHDR *upcomingHeader;
	int isFinished;
} PlayerState;

typedef struct ChunkGeneralInfo {
	char id[5];
	int size;
} ChunkGeneralInfo;

void CALLBACK waveOutCallback(
	HWAVEOUT handleWaveOut,
	UINT uMsg,
	DWORD_PTR dwUser, // PlayerState
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2
) {
	if (uMsg != WOM_DONE) return;
	PlayerState *playerState = (PlayerState*)dwUser;
	
	// Unprepare the old header and release resources
	if (playerState->headerToUnprepare != NULL) {
		MMRESULT waveOutUnprepareHeaderResult = waveOutUnprepareHeader(
			handleWaveOut,
			playerState->headerToUnprepare,
			sizeof(WAVEHDR)
		);
		if (waveOutUnprepareHeaderResult != MMSYSERR_NOERROR) {
			printf("waveOutUnprepareHeader error: %d\r\n", waveOutUnprepareHeaderResult);
		}
		free(playerState->headerToUnprepare->lpData);
		free(playerState->headerToUnprepare);
	}
	
	playerState->headerToUnprepare = playerState->upcomingHeader;
	
	// Read the next buffer
	void *buffer = malloc(playerState->bufferSize);
	memset(buffer, 0, playerState->bufferSize);
	size_t numberOfReadBytes = fread(
		buffer,
		1,
		playerState->bufferSize,
		playerState->file
	);
	if (numberOfReadBytes == 0) {
		printf("The end\r\n");
		playerState->isFinished = 1;
		return;
	}
	
	// Create a header for the new buffer and prepare it for playing
	WAVEHDR *waveHeader = malloc(sizeof(WAVEHDR));
	memset(waveHeader, 0, sizeof(WAVEHDR));
	waveHeader->lpData = buffer;
	waveHeader->dwBufferLength = playerState->bufferSize;
	
	MMRESULT waveOutPrepareHeaderResult = waveOutPrepareHeader(
		handleWaveOut,
		waveHeader,
		sizeof(WAVEHDR)
	);
	if (waveOutPrepareHeaderResult != MMSYSERR_NOERROR) {
		printf("waveOutPrepareHeader error: %d\r\n", waveOutPrepareHeaderResult);
		free(waveHeader);
		return;
	}
	playerState->upcomingHeader = waveHeader;
	
	// Put the buffer into the cue for playback
	MMRESULT waveOutWriteResult = waveOutWrite(
		handleWaveOut,
		playerState->upcomingHeader,
		sizeof(WAVEHDR)
	);
	if (waveOutWriteResult != MMSYSERR_NOERROR) {
		printf("waveOutWrite error: %d\r\n", waveOutWriteResult);
		waveOutCallback(handleWaveOut, WOM_DONE, (DWORD_PTR)playerState, 0, 0);
		return;
	}
}

ChunkGeneralInfo nextChunkGeneralInfo(FILE *wavFile) {
	ChunkGeneralInfo chunkInfo = {0};
	fgets((char * restrict)&(chunkInfo.id), sizeof_member(ChunkGeneralInfo, id), wavFile);
	fread(&(chunkInfo.size), sizeof_member(ChunkGeneralInfo, size), 1, wavFile);
	
	return chunkInfo;
}

void findNextChunk(FILE *wavFile, const char chunkId[5]) {
	ChunkGeneralInfo fmtSubchunkGeneralInfo = fmtSubchunkGeneralInfo = nextChunkGeneralInfo(wavFile);
	while (!streq(fmtSubchunkGeneralInfo.id, chunkId)) {
		printf("skipped chunk: %s of size %d\r\n", fmtSubchunkGeneralInfo.id, fmtSubchunkGeneralInfo.size);
		fseek(wavFile, fmtSubchunkGeneralInfo.size, SEEK_CUR);
		fmtSubchunkGeneralInfo = nextChunkGeneralInfo(wavFile);
	}
	printf("found chunk: %s of size %d\r\n", fmtSubchunkGeneralInfo.id, fmtSubchunkGeneralInfo.size);
}

BOOL selectWavFile(char *filePath, int filePathSize) {
	OPENFILENAMEA openFileName = {
		.lStructSize = sizeof(OPENFILENAMEA),
		.hwndOwner = NULL,
		.lpstrFilter = "WAV Files\0*.WAV\0All Files\0*.*\0",
		.lpstrCustomFilter = NULL,
		.nFilterIndex = 0,
		.lpstrFile = filePath,
		.nMaxFile = filePathSize,
		.lpstrFileTitle = NULL,
		.lpstrInitialDir = NULL,
		.lpstrTitle = "Select a wav file to be played",
		.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST,
	};
	return GetOpenFileNameA(&openFileName);
}

int main(int argc, char *argv[]) {
	char filePath[512];
	memset(filePath, 0, sizeof(filePath));
	if (argc > 1) {
		strncpy(filePath, argv[1], sizeof(filePath));
	} else {
		BOOL isSelected = selectWavFile(filePath, sizeof(filePath));
		if (!isSelected) {
			printf("No file selected.\r\n");
			return 3;
		}
	}
	printf("Selected file: %s\r\n", filePath);
	
	FILE *wavFile = fopen(filePath, "rb");
	
	char chunkId[5];
	fgets(chunkId, sizeof chunkId, wavFile);
	
	int chunkSize;
	fread(&chunkSize, sizeof chunkSize, 1, wavFile);
	
	char format[5];
	fgets(format, sizeof format, wavFile);
	
	// Subchunk 1
	
	findNextChunk(wavFile, "fmt ");

	short audioFormat;
	fread(&audioFormat, sizeof audioFormat, 1, wavFile);
	
	short numChannels;
	fread(&numChannels, sizeof numChannels, 1, wavFile);
	
	int sampleRate;
	fread(&sampleRate, sizeof sampleRate, 1, wavFile);
	
	int byteRate;
	fread(&byteRate, sizeof byteRate, 1, wavFile);
	
	short blockAlign;
	fread(&blockAlign, sizeof blockAlign, 1, wavFile);
	
	short bitsPerSample;
	fread(&bitsPerSample, sizeof bitsPerSample, 1, wavFile);
	
	// Subchunk 2
	
	findNextChunk(wavFile, "data");
	
	HWAVEOUT handleWaveOut;
	WAVEFORMATEX waveFormatEx = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = numChannels,
		.nSamplesPerSec = sampleRate,
		.nAvgBytesPerSec = byteRate,
		.nBlockAlign = blockAlign,
		.wBitsPerSample = bitsPerSample,
		.cbSize = 0
	};
	
	int bufferSize = blockAlign * 2048;
	// If specified, use the command line provided buffer size
	if (argc > 2) {
		bufferSize = blockAlign * atoi(argv[2]);
	}
	PlayerState playerState = { bufferSize, wavFile, NULL, NULL, 0 };
	
	MMRESULT waveOutOpenResult = waveOutOpen(
		&handleWaveOut,
		WAVE_MAPPER,
		&waveFormatEx,
		(DWORD_PTR)&waveOutCallback,
		(DWORD_PTR)&playerState, // User data
		CALLBACK_FUNCTION
	);
	if (waveOutOpenResult != MMSYSERR_NOERROR) {
		printf("waveOutOpen error: %d\r\n", waveOutOpenResult);
		return 2;
	}
	
	waveOutCallback(handleWaveOut, WOM_DONE, (DWORD_PTR)&playerState, 0, 0);
	waveOutCallback(handleWaveOut, WOM_DONE, (DWORD_PTR)&playerState, 0, 0);
	
	// Wait for the playback to finish to release the resources
	while (!playerState.isFinished) {
		Sleep(500);
	}
	
	waveOutClose(handleWaveOut);
	fclose(wavFile);
	
	return 0;
}