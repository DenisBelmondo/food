#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <umka_api.h>

#include "info.h"
#include "d_iwad.h"
#include "i_exit.h"
#include "w_wad.h"

#include "food.h"

enum
{
	DEFAULT_STACK_SIZE      =  1 * 1024 * 1024,  // Slots
	MAX_CALL_STACK_DEPTH    = 10,
	MAX_STR_LENGTH          = 256
};

static Umka *umka_;

static void PrintCompileWarning(UmkaError *warning)
{
	fprintf(stderr, "Warning %s (%d, %d): %s\n", warning->fileName, warning->line, warning->pos, warning->msg);
}

static void PrintCompileError(Umka *umka)
{
	const UmkaError *error = umkaGetError(umka);
	fprintf(stderr, "Error %s (%d, %d): %s\n", error->fileName, error->line, error->pos, error->msg);
}

static void PrintRuntimeError(Umka *umka)
{
	const UmkaError *error = umkaGetError(umka);

	if (error->msg[0])
	{
		fprintf(stderr, "\nRuntime error %s (%d): %s\n", error->fileName, error->line, error->msg);
		fprintf(stderr, "Terminated at:\n");

		for (int depth = 0; depth < MAX_CALL_STACK_DEPTH; depth++)
		{
			char fileName[MAX_STR_LENGTH + 1], fnName[MAX_STR_LENGTH + 1];
			int line;

			if (!umkaGetCallStack(umka, depth, MAX_STR_LENGTH + 1, NULL, fileName, fnName, &line))
				break;

			fprintf(stderr, "    %s: %s (%d)\n", fnName, fileName, line);
		}
	}
}

static void GetInfoContext(UmkaStackSlot *params, UmkaStackSlot *result)
{
	typedef UmkaDynArray(state_t) States;
	typedef UmkaDynArray(char *) Strings;
	typedef UmkaDynArray(mobjinfo_t) MobjInfos;

	typedef struct
	{
		States originalStates;
		Strings originalSprNames;
		MobjInfos originalMobjInfo;
		States states;
		Strings sprNames;
		MobjInfos mobjInfo;
		int zmt_ambientSound;
	} InfoContextResult;

	static InfoContextResult infoContextResult;

	const UmkaType *resultType = umkaGetResultType(params, result);
	const UmkaType *field0Type = umkaGetFieldType(resultType, "originalStates");

	umkaMakeDynArray(
		umka_,
		&infoContextResult.originalStates,
		field0Type,
		NUMSTATES);

	for (int i = 0; i < NUMSTATES; i++)
	{
		infoContextResult.originalStates.data[i] = original_states[i];
	}

	infoContextResult.zmt_ambientSound = zmt_ambientsound;

	umkaGetResult(params, result)->ptrVal = &infoContextResult;
}

void FoodInit(void)
{
	int lumpNum = W_GetNumForName("UMKA");
	int lumpLength = W_LumpLength(lumpNum);
	char *sourceString = malloc(lumpLength + 1);

	W_ReadLump(lumpNum, sourceString);
	sourceString[lumpLength] = '\0';

	I_AtExit(FoodShutdown, true);

	umka_ = umkaAlloc();

	bool ok = umkaInit(umka_, "main.um", sourceString, DEFAULT_STACK_SIZE, NULL, 0, NULL, false, true, PrintCompileWarning);

	if (!ok)
	{
		fprintf(stderr, "Could not initialize Umka.");
		return;
	}

	umkaAddFunc(umka_, "getInfoContext", GetInfoContext);

	ok = umkaCompile(umka_);

	if (!ok)
	{
		PrintCompileError(umka_);
		return;
	}

	ok = umkaRun(umka_) == 0;

	if (!ok)
	{
		PrintRuntimeError(umka_);
	}
}

void FoodShutdown(void)
{
	umkaFree(umka_);
}
