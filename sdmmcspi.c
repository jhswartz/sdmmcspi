#define _DEFAULT_SOURCE
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ERROR(message) fprintf(stderr, "%s\n\n", message)

volatile sig_atomic_t Interrupted = false;

bool     Interactive    = true;
bool     Verbose        = true;
char *   Device         = NULL;
int      Descriptor     = -1;
uint8_t  Mode           = 0;
uint8_t  BitsPerWord    = 8;
uint32_t ClockFrequency = 16000000;
uint16_t BlockLength    = 512;
uint32_t PollInterval   = 1000000;
bool     HighCapacity   = false;
bool     FaultTolerant  = false;
uint32_t RetryCount     = 0;

struct Command 
{
	uint8_t  type;
	uint32_t data;
	uint8_t  checksum;
};

enum R1
{
	Ready          = 0x00,
	Idle           = 0x01,
	EraseReset     = 0x02,
	IllegalCommand = 0x04,
	ChecksumError  = 0x08,
	EraseSeqError  = 0x10,
	AddressError   = 0x20,
	ParameterError = 0x40
};

struct R3
{
	enum R1  r1;
	uint32_t ocr;
};

#define OCR_VOLTAGE_LOW  0x00000080
#define OCR_VOLTAGE_2V8  0x00008000
#define OCR_VOLTAGE_2V9  0x00010000
#define OCR_VOLTAGE_3V0  0x00020000
#define OCR_VOLTAGE_3V1  0x00040000
#define OCR_VOLTAGE_3V2  0x00080000
#define OCR_VOLTAGE_3V3  0x00100000
#define OCR_VOLTAGE_3V4  0x00200000
#define OCR_VOLTAGE_3V5  0x00400000
#define OCR_VOLTAGE_3V6  0x00800000
#define OCR_CCS          0x40000000
#define OCR_BUSY         0x80000000

struct R7
{
	enum R1  r1;
	uint8_t  voltage;
	uint8_t  pattern;
};

struct CSD1
{
	uint8_t  taac;
	uint8_t  nsac;
	uint8_t  transferRate;
	uint16_t ccc;
	uint8_t  readBlockLength;
	bool	 readBlockPartial;
	bool     writeBlockMisalign;
	bool     readBlockMisalign;
	bool     dsr;
	uint16_t deviceSize;
	uint8_t  readCurrentVddMin;
	uint8_t  readCurrentVddMax;
	uint8_t  writeCurrentVddMin;
	uint8_t  writeCurrentVddMax;
	uint8_t  deviceSizeMultiplier;
	bool     eraseBlockEnable;
	uint8_t  eraseSectorSize;
	uint8_t  wpGroupSize;
	bool     wpGroupEnable;
	uint8_t  writeSpeedFactor;
	uint8_t  writeBlockLength;
	bool	 writeBlockPartial;
	bool	 fileFormatGroup;
	bool	 copy;
	bool	 wpPermanent;
	bool	 wpTemporary;
	uint8_t  fileFormat;
	uint8_t  checksum;
};

struct CSD2
{
	uint8_t  taac;
	uint8_t  nsac;
	uint8_t  transferRate;
	uint16_t ccc;
	uint8_t  readBlockLength;
	bool	 readBlockPartial;
	bool     writeBlockMisalign;
	bool     readBlockMisalign;
	bool     dsr;
	uint32_t deviceSize;
	bool     eraseBlockEnable;
	uint8_t  eraseSectorSize;
	uint8_t  wpGroupSize;
	bool     wpGroupEnable;
	uint8_t  writeSpeedFactor;
	uint8_t  writeBlockLength;
	bool	 writeBlockPartial;
	bool	 fileFormatGroup;
	bool	 copy;
	bool	 wpPermanent;
	bool	 wpTemporary;
	uint8_t  fileFormat;
	uint8_t  checksum;
};

union CSDx
{
	struct CSD1 csd1;
	struct CSD2 csd2;
};

enum CSDVersion
{
	CSD1 = 0,
	CSD2 = 1,
};

struct CSD
{
	enum  R1         r1;
	enum  CSDVersion version;
	union CSDx       data;
};

struct CID
{
	enum R1  r1;
	uint8_t  manufacturer;
	char     oem[2];
	char     product[5];
	uint8_t  majorRevision;
	uint8_t  minorRevision;
	uint32_t serialNumber;
	uint8_t  reserved;
	uint8_t  year;
	uint8_t  month;
	uint8_t  checksum;
};

enum BlockToken
{
	BlockError      = 0x01,
	BlockCCError    = 0x02,
	BlockECCFailure = 0x04,
	BlockOutOfRange = 0x08,
	BlockStart      = 0xfe
};

enum WriteStatus
{
	NotWritten    = 0x00,
	WriteAccepted = 0x02,
	WriteCRCError = 0x05,
	WriteError    = 0x06
};

struct Block
{
	enum R1         r1;
	enum BlockToken token; 
	uint8_t *       data;
	size_t          length;
	uint16_t        checksum;
};

enum ResponseType
{
	R1,
	R3,
	R7,
	CSD,
	CID,
	Status,
	Block
};

union ResponseData
{
	enum   R1    r1;
	struct R3    r3;
	struct R7    r7;
	struct CSD   csd;
	struct CID   cid;
	struct Block block;
};

struct Response
{
	enum  ResponseType type;
	union ResponseData data;
};

static void interrupt();
static void interact(void);
static void displayPrompt(void);
static void parseCommand(char *);
static int match(char **, char *);

static void displayCommands(void);
static void displaySessionParameters(void);

static int acceptCommand0(void);
static int acceptCommand1(void);
static int acceptCommand6(char **);
static int acceptCommand8(char **);
static int acceptCommand9(void);
static int acceptCommand10(void);
static int acceptCommand16(char **);
static int acceptCommand17(char **);
static int acceptCommand58(void);
static int acceptApplicationCommand41(char **);
static int acceptRetryCommand(char **);
static int acceptPushCommand(char **);
static int acceptPullCommand(char **);

static int push(char *, uint32_t);
static int pull(uint32_t, uint32_t, char *);
static void nextBlock(uint32_t *);
static int countBlocks(FILE *, size_t *);
static void printBadBlockWarning(uint32_t);

static int setMode(void);
static int setBitsPerWord(void);
static int parseClockFrequency(char *);
static int setClockFrequency(void);
static int parseDevice(char *);
static int openDevice(void);
static bool isDeviceOpen(void);
static void closeDevice(void);

static int receiveData(uint8_t *, size_t);
static int transmitData(uint8_t *, size_t);
static int exchangeData(struct spi_ioc_transfer *);

static int command(uint8_t, uint32_t,
                   enum ResponseType, struct Response *);
static int transmitCommand(uint8_t, uint32_t);
static void serialiseCommand(struct Command *, uint8_t *);

static uint8_t calculateCRC7(uint8_t *, size_t);
static void calculateCRC16(uint8_t *, size_t, uint16_t *);

static int receiveResponse(enum ResponseType, struct Response *);
static int receiveR1(enum R1 *);
static int receiveR3(struct R3 *);
static int receiveR7(struct R7 *);
static int receiveCSD(struct CSD *);
static int receiveCID(struct CID *);
static int receiveBlock(size_t, struct Block *);

static int transmitBlock(struct Block *);
static int receiveWriteStatus(enum WriteStatus *);

static void dumpCommand(struct Command *);
static void dumpR1(enum R1 *);
static void dumpR3(struct R3 *);
static void dumpR7(struct R7 *);
static void dumpCSD(struct CSD *);
static void dumpCSD1(struct CSD1 *);
static void dumpCSD2(struct CSD2 *);
static void dumpCID(struct CID *);
static void dumpWriteStatus(enum WriteStatus *);
static void displayBlockToken(struct Block *);
static void displayBlockChecksum(struct Block *);
static void displayString(char *, char *);
static void displaySubstring(char *, char *, size_t);
static void displayDate(char *, uint16_t, uint16_t);
static void displayVersion(char *, uint8_t, uint8_t);
static void displayFlag(char *, uint8_t);
static void displayFrequency(char *, uint32_t);
static void displayMiliseconds(char *, uint32_t);
static void display8(char *, uint8_t);
static void describe8(char *, uint8_t, char *);
static void display16(char *, uint16_t);
static void display32(char *, uint32_t);
static void describe32(char *, uint32_t, char *);
static void dump(uint8_t *, size_t, FILE *);

static uint32_t slice(uint8_t *, int, int);

static int parseUInt16(char **, uint16_t *);
static int parseUInt32(char **, uint32_t *);
static int parseFilename(char **, char **);

int main(int argc, char *argv[])
{
	interact();
	return EXIT_SUCCESS;
}

static void interact(void)
{
	char buffer[BUFSIZ];

	while (Interactive)
	{
		displayPrompt();

		if (fgets(buffer, sizeof(buffer), stdin) == NULL)
		{
			if (ferror(stdin))
			{
				ERROR(strerror(errno));
			}

			else if (feof(stdin))
			{
				Interactive = false;
			}

			break;
		}

		parseCommand(buffer);
	}

	closeDevice();
}

static void displayPrompt(void)
{
	printf("sdmmc/spi> ");
}

static void parseCommand(char *cursor)
{
	if (match(&cursor, "?\n") == 0)
	{
		displayCommands();
	}

	else if (match(&cursor, "verbose\n") == 0)
	{
		Verbose = true;
	}

	else if (match(&cursor, "quiet\n") == 0)
	{
		Verbose = false;
	}

	else if (match(&cursor, "bye\n") == 0)
	{
		Interactive = false;
	}

	else if (match(&cursor, "session?\n") == 0)
	{
		displaySessionParameters();
	}

	else if (match(&cursor, "clock ") == 0)
	{
		if (parseClockFrequency(cursor) == -1)
		{
			ERROR("Invalid clock frequency");
		}

		else if (setClockFrequency() == -1)
		{
			ERROR(strerror(errno));
		}
	}

	else if (match(&cursor, "open ") == 0)
	{
		if (parseDevice(cursor) == -1)
		{
			ERROR("Invalid device");
		}

		else if (openDevice() == -1)
		{
			ERROR(strerror(errno));
		}
	}

	else if (match(&cursor, "close\n") == 0)
	{
		closeDevice();
	}

	else if (match(&cursor, "cmd0\n") == 0)
	{
		acceptCommand0();
	}

	else if (match(&cursor, "cmd1\n") == 0)
	{
		acceptCommand1();
	}

	else if (match(&cursor, "cmd6 ") == 0)
	{
		acceptCommand6(&cursor);
	}

	else if (match(&cursor, "cmd8 ") == 0)
	{
		acceptCommand8(&cursor);
	}

	else if (match(&cursor, "cmd9\n") == 0)
	{
		acceptCommand9();
	}

	else if (match(&cursor, "cmd10\n") == 0)
	{
		acceptCommand10();
	}

	else if (match(&cursor, "cmd16 ") == 0)
	{
		acceptCommand16(&cursor);
	}

	else if (match(&cursor, "cmd17 ") == 0)
	{
		acceptCommand17(&cursor);
	}

	else if (match(&cursor, "cmd58\n") == 0)
	{
		acceptCommand58();
	}

	else if (match(&cursor, "acmd41 ") == 0)
	{
		acceptApplicationCommand41(&cursor);
	}

	else if (match(&cursor, "fault tolerant\n") == 0)
	{
		FaultTolerant = true;
	}

	else if (match(&cursor, "fault intolerant\n") == 0)
	{
		FaultTolerant = false;
	}

	else if (match(&cursor, "retry ") == 0)
	{
		acceptRetryCommand(&cursor);
	}

	else if (match(&cursor, "push ") == 0)
	{
		acceptPushCommand(&cursor);
	}

	else if (match(&cursor, "pull ") == 0)
	{
		acceptPullCommand(&cursor);
	}

	else
	{
		ERROR("Unrecognised command");
	}
}

static int match(char **cursor, char *token)
{
	size_t length = strlen(token);

	if (strncmp(*cursor, token, length) == 0)
	{
		*cursor += length;
		return 0;
	}

	return -1;
}

static void displayCommands(void)
{
	displayString("?", "Display commands");
	displayString("session?", "Display session parameters");
	displayString("verbose", "Be verbose (default)");
	displayString("quiet", "Be quiet");
	displayString("bye", "Leave sdmmc/spi\n");
	displayString("clock FREQUENCY", "Set SPI clock frequency");
	displayString("open FILENAME", "Open SPI device");
	displayString("close", "Close SPI device\n");
	displayString("cmd0", "Go to Idle State");
	displayString("cmd1", "Send Operating Condition");
	displayString("cmd6 FUNCTION", "Check/Switch Function");
	displayString("cmd8 CONDITION", "Send Interface Condition");
	displayString("cmd9", "Read CSD Register");
	displayString("cmd10", "Read CID Register");
	displayString("cmd16 LENGTH", "Set Block Length");
	displayString("cmd17 ADDRESS", "Read Single Block");
	displayString("cmd58", "Read Operating Condition");
	displayString("acmd41 CONDITION", "Send Operating Condition\n");
	displayString("fault tolerant", "Pad and skip block on error");
	displayString("fault intolerant", "Abort on block error");
	displayString("retry COUNT", "Set block retry count\n");
	displayString("push FILE BLOCK", "Push blocks to card");
	displayString("pull BLOCK COUNT FILE", "Pull blocks from card\n");
}

static void displaySessionParameters(void)
{
	displayString("Device", Device);
	displayFrequency("Clock Frequency", ClockFrequency);
	displayMiliseconds("Poll Interval", PollInterval / 1000);
	displayString("Fault Tolerant?", FaultTolerant ? "Yes" : "No");
	display8("Retry Count", RetryCount);
	displayString("High Capacity?", HighCapacity ? "Yes" : "No");
	putchar('\n');
}

static int acceptCommand0(void)
{
	struct Response response;

	if (command(0, 0, R1, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand1(void)
{
	struct Response response;

	do
	{
		if (command(1, 0, R1, &response) == -1)
		{
			ERROR(strerror(errno));
			return -1;
		}

		usleep(PollInterval);
	}
	while (response.data.r1 != Ready);

	return 0;
}

static int acceptCommand6(char **cursor)
{
	struct Response response;
	uint32_t data = 0;

	if (parseUInt32(cursor, &data) == -1)
	{
		ERROR("Invalid condition");
		return -1;
	}

	if (command(6, data, Status, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand8(char **cursor)
{
	struct Response response;
	uint32_t data = 0;

	if (parseUInt32(cursor, &data) == -1)
	{
		ERROR("Invalid condition");
		return -1;
	}

	if (command(8, data, R7, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand9(void)
{
	struct Response response;

	if (command(9, 0, CSD, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand10(void)
{
	struct Response response;

	if (command(10, 0, CID, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand16(char **cursor)
{
	struct Response response;
	
	if (parseUInt16(cursor, &BlockLength) == -1)
	{
		ERROR("Invalid block length");
		return -1;
	}

	if (command(16, BlockLength, R1, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptCommand17(char **cursor)
{
	struct Response response;
	uint32_t data = 0;

	if (parseUInt32(cursor, &data) == -1)
	{
		ERROR("Invalid address");
		return -1;
	}

	if (command(17, data, Block, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	free(response.data.block.data);
	return 0;
}

static int acceptCommand58(void)
{
	struct Response response;

	if (command(58, 0, R3, &response) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	return 0;
}

static int acceptApplicationCommand41(char **cursor)
{
	struct Response response;
	uint32_t data = 0;

	if (parseUInt32(cursor, &data) == -1)
	{
		ERROR("Invalid condition");
		return -1;
	}

	do
	{
		if (command(55, 0, R1, &response) == -1)
		{
			ERROR(strerror(errno));
			return -1;
		}

		if (command(41, data, R1, &response) == -1)
		{
			ERROR(strerror(errno));
			return -1;
		}

		usleep(PollInterval);
	}
	while (response.data.r1 != Ready);

	return 0;
}

static int acceptRetryCommand(char **cursor)
{
	return parseUInt32(cursor, &RetryCount);
}

static int acceptPushCommand(char **cursor)
{
	char *filename = NULL;
	uint32_t address = 0;

	if (parseFilename(cursor, &filename) == -1)
	{
		ERROR("Invalid filename");
		return -1;
	}

	if (parseUInt32(cursor, &address) == -1)
	{
		ERROR("Invalid address");
		return -1;
	}

	return push(filename, address);
}

static int acceptPullCommand(char **cursor)
{
	uint32_t address = 0;
	uint32_t count = 0;
	char *filename = NULL;

	if (parseUInt32(cursor, &address) == -1)
	{
		ERROR("Invalid address");
		return -1;
	}

	if (parseUInt32(cursor, &count) == -1)
	{
		ERROR("Invalid count");
		return -1;
	}

	if (parseFilename(cursor, &filename) == -1)
	{
		ERROR("Invalid filename");
		return -1;
	}

	return pull(address, count, filename);
}

static int push(char *filename, uint32_t address)
{
	int status = 0;
	int retries = 0;
	int delta = 0;
	size_t index = 0;
	size_t count = 0;
	size_t length = 0;
	time_t start, end;
	uint8_t buffer[BlockLength]; 
	struct Block block = {0};
	struct Response response = {0};
	enum WriteStatus writeStatus = NotWritten;
	FILE *file = NULL;

	start = time(NULL);
	file = fopen(filename, "r");

	if (file == NULL)
	{
		ERROR(strerror(errno));
		return -1;
	}

	if (countBlocks(file, &count) == -1)
	{
		ERROR(strerror(errno));
		return -1;
	}

	if (!HighCapacity)
	{
		address *= BlockLength;
	}

	signal(SIGINT, interrupt);

	while (index < count)
	{
		memset(buffer, 0, sizeof(buffer));
		length = fread(buffer, 1, sizeof(buffer), file);

		if (length < BlockLength)
		{
			if (ferror(file))
			{
				status = -1;
				ERROR(strerror(errno));
				break;
			}

			if (feof(file))
			{
				status = -1;
				ERROR("File truncated");
				break;
			}
		}

		block.token  = BlockStart;
		block.data   = buffer;
		block.length = sizeof(buffer);

		if (command(24, address, R1, &response) == -1)
		{
			status = -1;
			ERROR(strerror(errno));
			break;
		}

		if (response.data.r1 != Ready)
		{
			if (retries < RetryCount)
			{
				retries++;
				continue;
			}

			else
			{
				printBadBlockWarning(address);
				retries = 0;
				break;
			}
		}

		if (transmitBlock(&block) == -1)
		{
			status = -1;
			ERROR(strerror(errno));
			break;
		}

		if (receiveWriteStatus(&writeStatus) == -1)
		{
			status = -1;
			ERROR(strerror(errno));
			break;
		}

		if (writeStatus != WriteAccepted)
		{
			if (retries < RetryCount)
			{
				retries++;
				continue;
			}

			else
			{
				printBadBlockWarning(address);
				retries = 0;
				break;
			}
		}

		if (Interrupted)
		{
			Interrupted = false;
			break;
		}

		nextBlock(&address);
		index++;
	}

	fclose(file);
	signal(SIGINT, SIG_DFL);

	end = time(NULL);
	delta = difftime(end, start) + 1;
	printf("Pushed %d of %d block(s) in +-%ds\n\n", index, count, delta);

	return status;
}

static int pull(uint32_t address, uint32_t count, char *filename)
{
	int status = 0;
	int retries = 0;
	int delta = 0;
	uint32_t index = 0;
	time_t start, end;
	struct Response response;
	struct Block *block = NULL;
	FILE *file = NULL;

	start = time(NULL);
	file = fopen(filename, "w");

	if (file == NULL)
	{
		ERROR(strerror(errno));
		return -1;
	}

	if (!HighCapacity)
	{
		address *= BlockLength;
	}

	signal(SIGINT, interrupt);

	while (index < count)
	{
		if (command(17, address, Block, &response) == -1)
		{
			status = -1;
			ERROR(strerror(errno));
			break;
		}

		block = &response.data.block;

		if (block->r1 != Ready || block->token != BlockStart)
		{
			if (retries < RetryCount)
			{
				retries++;
				continue;
			}

			printBadBlockWarning(address);
			retries = 0;

			if (!FaultTolerant)
			{
				index--;
				free(block->data);
				break;
			}

			block->length = BlockLength;
			block->data = calloc(1, block->length);

			if (block->data == NULL)
			{
				status = -1;
				ERROR(strerror(errno));
				break;
			}
		}

		if (fwrite(block->data, block->length, 1, file) < 1)
		{
			status = -1;
			ERROR(strerror(errno));
			free(block->data);
			break;
		}

		free(block->data);

		if (Interrupted)
		{
			Interrupted = false;
			break;
		}

		nextBlock(&address);
		index++;
	}

	fclose(file);
	signal(SIGINT, SIG_DFL);

	end = time(NULL);
	delta = difftime(end, start) + 1;
	printf("Pulled %d of %d block(s) in +-%ds\n\n", index, count, delta);

	return status;
}

static void nextBlock(uint32_t *address)
{
	if (HighCapacity)
	{
		(*address)++;
	}

	else
	{
		*address += BlockLength; 
	}
}

static int countBlocks(FILE *file, size_t *count)
{
	long offset = 0;

	if (fseek(file, 0, SEEK_END) == -1)
	{
		return -1;
	}

	offset = ftell(file);

	if (offset == -1)
	{
		return -1;
	}

	if (fseek(file, 0, SEEK_SET) == -1)
	{
		return -1;
	}

	*count = offset / BlockLength;
	
	if (offset % BlockLength)
	{
		(*count)++;
	}

	return 0;
}

static void printBadBlockWarning(uint32_t address)
{
	if (!HighCapacity)
	{
		address /= BlockLength;
	}

	fprintf(stderr, "Bad Block: %d\n", address);
}

static void interrupt()
{
	Interrupted = true;
}

static int setMode(void)
{
	return ioctl(Descriptor, SPI_IOC_WR_MODE, &Mode);
}

static int setBitsPerWord(void)
{
	return ioctl(Descriptor, SPI_IOC_WR_BITS_PER_WORD, &BitsPerWord);
}

static int parseClockFrequency(char *argument)
{
	return parseUInt32(&argument, &ClockFrequency);
}

static int setClockFrequency(void)
{
	return ioctl(Descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &ClockFrequency);
}

static int parseDevice(char *argument)
{
	char *filename = NULL;

	if (Device != NULL)
	{
		free(Device);
	}

	if (parseFilename(&argument, &filename) == -1)
	{
		return -1;
	}

	Device = strdup(filename);

	if (Device == NULL)
	{
		return -1;
	}

	return 0;
}

static int openDevice(void)
{
	if (isDeviceOpen())
	{
		closeDevice();
	}

	Descriptor = open(Device, O_RDWR);

	if (Descriptor == -1)
	{
		return -1;
	}

	if (setMode() == -1)
	{
		closeDevice();
		return -1;
	}

	if (setBitsPerWord() == -1)
	{
		closeDevice();
		return -1;
	}

	if (setClockFrequency() == -1)
	{
		closeDevice();
		return -1;
	}

	return 0;
}

static bool isDeviceOpen(void)
{
	return Descriptor > -1;
}

static void closeDevice(void)
{
	if (isDeviceOpen())
	{
		close(Descriptor);
	}

	Descriptor = -1;

	if (Device != NULL)
	{
		free(Device);
		Device = NULL;
	}
}

static int transmitData(uint8_t *request, size_t length)
{
	uint8_t response[length];

	struct spi_ioc_transfer transfer =
	{
		.speed_hz = ClockFrequency,
		.tx_buf   = (uintptr_t)request,
		.rx_buf   = (uintptr_t)response,
		.len      = length 
	};

	memset(response, 0xff, length);

	if (exchangeData(&transfer) == -1)
	{
		return -1;
	}

	if (Verbose)
	{
		printf("TX\n");
		dump(request, length, stdout);
	}

	return 0;
}

static int receiveData(uint8_t *response, size_t length)
{
	uint8_t request[length];

	struct spi_ioc_transfer transfer =
	{
		.speed_hz = ClockFrequency,
		.tx_buf = (uintptr_t)request,
		.rx_buf = (uintptr_t)response,
		.len    = 1
	};

	memset(response, 0xff, length);
	memset(request, 0xff, length);

	for (size_t index = 0; index < length; index++)
	{
		while (response[index] == 0xff)
		{
			if (exchangeData(&transfer) == -1)
			{
				return -1;
			}

			if (index > 0)
			{
				break;
			}
		}

		transfer.tx_buf++;
		transfer.rx_buf++;
	}

	if (Verbose)
	{
		printf("RX\n");
		dump(response, length, stdout);
	}	

	return 0;
}

static int exchangeData(struct spi_ioc_transfer *transfer)
{
	return ioctl(Descriptor, SPI_IOC_MESSAGE(1), transfer);
}

static int command(uint8_t commandType, uint32_t data,
                   enum ResponseType responseType, struct Response *response)
{
	if (transmitCommand(commandType, data) == -1)
	{
		return -1;
	}

	if (receiveResponse(responseType, response) == -1)
	{
		return -1;
	}

	return 0;
}


static int transmitCommand(uint8_t type, uint32_t data)
{
	uint8_t buffer[7];
	struct Command command = { type, data };

	serialiseCommand(&command, buffer);

	if (transmitData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	if (Verbose)
	{
		dumpCommand(&command);
	}

	return 0;
}

static void serialiseCommand(struct Command *command, uint8_t *buffer)
{
	enum Delimiters
	{
		Synchronisation = 255,
		Transmission    = 64,
		Termination     = 1
	};

	buffer[0] = Synchronisation;
	buffer[1] = Transmission | command->type;
	buffer[2] = command->data >> 24;
	buffer[3] = command->data >> 16;
	buffer[4] = command->data >>  8;
	buffer[5] = command->data;

	command->checksum = calculateCRC7(buffer + 1, 5);
	buffer[6] = (command->checksum << 1) | Termination;
}

static uint8_t calculateCRC7(uint8_t *data, size_t size)
{
	uint8_t crc = 0;
	uint8_t byte  = 0;

	for (size_t index = 0; index < size; index++)
	{
		byte  = data[index] ^ crc;
		crc = byte >> 4;
		byte  = byte ^ (crc ^ (crc >> 3));
		crc = (byte << 1) ^ (byte << 4);
	}

	return crc >> 1;
}

static void dumpCommand(struct Command *command)
{
	char *label = "Unknown";

	switch (command->type)
	{
		case 0:
			label = "Go to Idle State";
			break;

		case 1:
			label = "Send Operating Condition";
			break;

		case 6:
			label = "Check / Switch Card Function";
			break;

		case 8:
			label = "Send Interface Condition";
			break;

		case 9:
			label = "Read CSD Register";
			break;

		case 10:
			label = "Read CID Register";
			break;

		case 16:
			label = "Set Block Length";
			break;

		case 17:
			label = "Read Single Block";
			break;

		case 41:
			label = "Send Operating Condition";
			break;

		case 55:
			label = "Begin Application Specific Command";
			break;

		case 58:
			label = "Read Operating Condition";
			break;

		default:
			break;
	}

	describe8("Command Type", command->type, label);
	display32("Command Data", command->data);
	display8("Command Checksum", command->checksum);
	putchar('\n');
}

static int receiveResponse(enum ResponseType type, struct Response *response)
{
	union ResponseData *data = &response->data;

	switch (type)
	{
		case R1:
			response->type = R1;
			return receiveR1(&data->r1);

		case R3:
			response->type = R3;
			return receiveR3(&data->r3);

		case R7:
			response->type = R7;
			return receiveR7(&data->r7);

		case CSD:
			response->type = CSD;
			return receiveCSD(&data->csd);

		case CID:
			response->type = CID;
			return receiveCID(&data->cid);

		case Status:
			response->type = Block;
			return receiveBlock(64, &data->block);

		case Block:
			response->type = Block;
			return receiveBlock(BlockLength, &data->block);

		default:
			break;
	}

	return -1;
}

static int receiveR1(enum R1 *r1)
{
	uint8_t buffer[1];

	if (receiveData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	*r1 = buffer[0];

	if (Verbose)
	{
		dumpR1(r1);
	}

	return 0;
}

static int receiveR3(struct R3 *r3)
{
	uint8_t buffer[4];

	if (receiveR1(&r3->r1) == -1)
	{
		return -1;
	}

	if (r3->r1 != Ready)
	{
		return 0;
	}

	if (receiveData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	r3->ocr = slice(buffer, 0, 32);
	HighCapacity = r3->ocr & OCR_CCS;

	if (Verbose)
	{
		dumpR3(r3);
	}

	return 0;
}

static int receiveR7(struct R7 *r7)
{
	uint8_t buffer[4];

	if (receiveR1(&r7->r1) == -1)
	{
		return -1;
	}

	if (r7->r1 != Idle)
	{
		return 0;
	}

	if (receiveData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	r7->voltage = slice(buffer, 20, 4);
	r7->pattern = slice(buffer, 24, 8);

	if (Verbose)
	{
		dumpR7(r7);
	}

	return 0;
}

static int receiveCSD(struct CSD *csd)
{
	struct Block block;
	struct CSD1 *csd1;
	struct CSD2 *csd2;

	if (receiveBlock(16, &block) == -1)
	{
		return -1;
	}

	csd->r1      = block.r1;
	csd->version = slice(block.data, 0, 2);

	if (csd->version == CSD1)
	{
		csd1                       = &csd->data.csd1;
		csd1->taac                 = slice(block.data,   8,  8);
		csd1->nsac                 = slice(block.data,  16,  8);
		csd1->transferRate         = slice(block.data,  24,  8);
		csd1->ccc                  = slice(block.data,  32, 12);
		csd1->readBlockLength      = slice(block.data,  44,  4);
		csd1->readBlockPartial     = slice(block.data,  48,  1);
		csd1->writeBlockMisalign   = slice(block.data,  49,  1);
		csd1->readBlockMisalign    = slice(block.data,  50,  1);
		csd1->dsr                  = slice(block.data,  51,  1);
		csd1->deviceSize           = slice(block.data,  54, 12);
		csd1->readCurrentVddMin    = slice(block.data,  66,  3);
		csd1->readCurrentVddMax    = slice(block.data,  69,  3);
		csd1->writeCurrentVddMin   = slice(block.data,  72,  3);
		csd1->writeCurrentVddMax   = slice(block.data,  75,  3);
		csd1->deviceSizeMultiplier = slice(block.data,  78,  3);
		csd1->eraseBlockEnable     = slice(block.data,  81,  1);
		csd1->eraseSectorSize      = slice(block.data,  82,  7);
		csd1->wpGroupSize          = slice(block.data,  89,  7);
		csd1->wpGroupEnable        = slice(block.data,  96,  1);
		csd1->writeSpeedFactor     = slice(block.data,  99,  3);
		csd1->writeBlockLength     = slice(block.data, 102,  4);
		csd1->writeBlockPartial    = slice(block.data, 106,  1);
		csd1->fileFormatGroup      = slice(block.data, 112,  1);
		csd1->copy                 = slice(block.data, 113,  1);
		csd1->wpPermanent          = slice(block.data, 114,  1);
		csd1->wpTemporary          = slice(block.data, 115,  1);
		csd1->fileFormat           = slice(block.data, 116,  2);
		csd1->checksum             = slice(block.data, 120,  7);
	}

	else if (csd->version == CSD2)
	{
		csd2                     = &csd->data.csd2;
		csd2->taac               = slice(block.data,   8,  8);
		csd2->nsac               = slice(block.data,  16,  8);
		csd2->transferRate       = slice(block.data,  24,  8);
		csd2->ccc                = slice(block.data,  32, 12);
		csd2->readBlockLength    = slice(block.data,  44,  4);
		csd2->readBlockPartial   = slice(block.data,  48,  1);
		csd2->writeBlockMisalign = slice(block.data,  49,  1);
		csd2->readBlockMisalign  = slice(block.data,  50,  1);
		csd2->dsr                = slice(block.data,  51,  1);
		csd2->deviceSize         = slice(block.data,  58, 22);
		csd2->eraseBlockEnable   = slice(block.data,  81,  1);
		csd2->eraseSectorSize    = slice(block.data,  82,  7);
		csd2->wpGroupSize        = slice(block.data,  89,  7);
		csd2->wpGroupEnable      = slice(block.data,  96,  1);
		csd2->writeSpeedFactor   = slice(block.data,  99,  3);
		csd2->writeBlockLength   = slice(block.data, 102,  4);
		csd2->writeBlockPartial  = slice(block.data, 106,  1);
		csd2->fileFormatGroup    = slice(block.data, 112,  1);
		csd2->copy               = slice(block.data, 113,  1);
		csd2->wpPermanent        = slice(block.data, 114,  1);
		csd2->wpTemporary        = slice(block.data, 115,  1);
		csd2->fileFormat         = slice(block.data, 116,  2);
		csd2->checksum           = slice(block.data, 120,  1);
	}

	free(block.data);

	if (Verbose)
	{
		dumpCSD(csd);
	}

	return 0;
}

static int receiveCID(struct CID *cid)
{
	struct Block block;

	if (receiveBlock(16, &block) == -1)
	{
		return -1;
	}

	cid->r1           = block.r1;
	cid->manufacturer = block.data[0];

	memcpy(cid->oem,     block.data + 1, 2);
	memcpy(cid->product, block.data + 3, 5);

	cid->majorRevision = slice(block.data,  64,  4);
	cid->minorRevision = slice(block.data,  68,  4);
	cid->serialNumber  = slice(block.data,  72, 32); 
	cid->reserved      = slice(block.data, 104,  4);
	cid->year          = slice(block.data, 108,  8);
	cid->month         = slice(block.data, 116,  4);
	cid->checksum      = slice(block.data, 120,  7);

	free(block.data);

	if (Verbose)
	{
		dumpCID(cid);
	}

	return 0;
}

static int receiveBlock(size_t length, struct Block *block)
{
	uint8_t buffer[1 + length + 2];

	memset(block, 0, sizeof(*block));

	if (receiveR1(&block->r1) == -1)
	{
		return -1;
	}

	if (block->r1 != Ready)
	{
		return -1;
	}

	if (receiveData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	block->token = buffer[0];

	if (Verbose)
	{
		displayBlockToken(block);
	}

	if (block->token != BlockStart)
	{
		return 0;
	}

	block->data = calloc(1, length);

	if (block->data == NULL)
	{
		return -1;
	}

	memcpy(block->data, buffer + 1, length);
	block->length = length;
	block->checksum = slice(buffer + 1 + length, 0, 16);

	if (Verbose)
	{
		displayBlockChecksum(block);
	}

	return 0;
}

static int transmitBlock(struct Block *block)
{
	uint8_t buffer[1 + block->length];

	memcpy(buffer, &block->token, 1);
	memcpy(buffer + 1, block->data, block->length);

	if (transmitData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	if (Verbose)
	{
		displayBlockToken(block);
	}

	return 0;
}

static int receiveWriteStatus(enum WriteStatus *writeStatus)
{
	uint8_t buffer[1];

	if (receiveData(buffer, sizeof(buffer)) == -1)
	{
		return -1;
	}

	*writeStatus = (buffer[0] >> 1) & 0x07;

	if (Verbose)
	{
		dumpWriteStatus(writeStatus);
	}

	if (*writeStatus == WriteAccepted)
	{
		do
		{
			if (receiveData(buffer, sizeof(buffer)) == -1)
			{
				return -1;
			}
		}
		while (*buffer == NotWritten);
	}

	return 0;
}

static void calculateCRC16(uint8_t *data, size_t length, uint16_t *checksum)
{
	static uint16_t lookup[] =
	{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
	};

	for (size_t index = 0; index < length; index++)
	{
		uint8_t byte = data[index];
		*checksum = (*checksum << 8) ^ lookup[*checksum >> 8 ^ byte];
	}
}

static void dumpR1(enum R1 *r1)
{
	char *label = "Unknown";

	switch (*r1)
	{
		case Ready:
			label = "Ready";
			break;

		case Idle:
			label = "Idle";
			break;

		case EraseReset:
			label = "Erase/Reset";
			break;

		case IllegalCommand:
			label = "Illegal Command";
			break;

		case ChecksumError:
			label = "Checksum Error";
			break;

		case EraseSeqError:
			label = "Erase Sequence Error";
			break;

		case AddressError:
			label = "Address Error";
			break;

		case ParameterError:
			label = "Parameter Error";
			break;

		default:
			break;
	}

	describe8("Card State", *r1, label);
	putchar('\n');
}

static void dumpR3(struct R3 *r3)
{
	if (r3->ocr & OCR_BUSY)
	{
		describe32("OCR", OCR_BUSY, "Busy");
	}

	else
	{
		describe32("OCR", 0, "Idle");
	}

	if (r3->ocr & OCR_CCS)
	{
		describe32("", OCR_CCS, "High Capacity");
	}

	else
	{
		describe32("", 0, "Standard Capacity");
	}

	if (r3->ocr & OCR_VOLTAGE_3V6)
	{
		describe32("", OCR_VOLTAGE_3V6, "3.5V - 3.6V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V5)
	{
		describe32("", OCR_VOLTAGE_3V5, "3.4V - 3.5V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V4)
	{
		describe32("", OCR_VOLTAGE_3V4, "3.3V - 3.4V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V3)
	{
		describe32("", OCR_VOLTAGE_3V3, "3.2V - 3.3V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V2)
	{
		describe32("", OCR_VOLTAGE_3V2, "3.1V - 3.2V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V1)
	{
		describe32("", OCR_VOLTAGE_3V1, "3.0V - 3.1V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_3V0)
	{
		describe32("", OCR_VOLTAGE_3V0, "2.9V - 3.0V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_2V9)
	{
		describe32("", OCR_VOLTAGE_2V9, "2.8V - 2.9V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_2V8)
	{
		describe32("", OCR_VOLTAGE_2V8, "2.7V - 2.8V OK");
	}

	if (r3->ocr & OCR_VOLTAGE_LOW)
	{
		describe32("", OCR_VOLTAGE_LOW, "Low Voltage OK");
	}

	putchar('\n');
}

static void dumpR7(struct R7 *r7)
{
	char *voltageLabel = "Unknown";

	switch (r7->voltage)
	{
		case 1:
			voltageLabel = "2.7V - 3.6V";
			break;

		case 2:
			voltageLabel = "Low Voltage";
			break;
	}

	describe8("Voltage Accepted", r7->voltage, voltageLabel);
	display8("Check Pattern", r7->pattern);
	putchar('\n');
}

static void dumpCSD(struct CSD *csd)
{
	switch (csd->version)
	{
		case CSD1:
			dumpCSD1(&csd->data.csd1);
			break;

		case CSD2:
			dumpCSD2(&csd->data.csd2);
			break;
	}
}

static void dumpCSD1(struct CSD1 *csd1)
{
	displayVersion("CSD Version", 1, 0);
	display8("TAAC", csd1->taac);
	display8("NSAC", csd1->nsac);
	display8("Maximum Transfer Rate", csd1->transferRate);
	display16("Command Classes", csd1->ccc);
	display8("Maximum Read Block Length", csd1->readBlockLength);
	displayFlag("Partial Block Reads?", csd1->readBlockPartial);
	displayFlag("Write Block Misalignment?", csd1->writeBlockMisalign);
	displayFlag("Read Block Misalignment?", csd1->readBlockMisalign);
	display8("DSR Implemented", csd1->dsr);
	display16("Device Size", csd1->deviceSize);
	display8("Max Read Current @ min(Vdd)", csd1->readCurrentVddMin);
	display8("Max Read Current @ max(Vdd)", csd1->readCurrentVddMax);
	display8("Max Write Current @ min(Vdd)", csd1->writeCurrentVddMin);
	display8("Max Write Current @ max(Vdd)", csd1->writeCurrentVddMax);
	display8("Device Size Multiplier", csd1->deviceSizeMultiplier);
	displayFlag("Erase Block Enabled?", csd1->eraseBlockEnable);
	display8("Erase Sector Size", csd1->eraseSectorSize);
	display8("Write Protect Group Size", csd1->wpGroupSize);
	displayFlag("Write Protect Group Enabled?", csd1->wpGroupEnable);
	display8("Write Speed Factor", csd1->writeSpeedFactor);
	display8("Max Write Block Length", csd1->writeBlockLength);
	displayFlag("Partial Block Writes?", csd1->writeBlockPartial);
	display8("File Format Group", csd1->fileFormatGroup);
	displayFlag("Copy?", csd1->copy);
	displayFlag("Permanent Write Protection?", csd1->wpPermanent);
	displayFlag("Temporary Write Protection?", csd1->wpTemporary);
	display8("File Format", csd1->fileFormat);
	display8("CSD Checksum", csd1->checksum);
	putchar('\n');
}

static void dumpCSD2(struct CSD2 *csd2)
{
	displayVersion("CSD Version", 2, 0);
	display8("TAAC", csd2->taac);
	display8("NSAC", csd2->nsac);
	display8("Maximum Transfer Rate", csd2->transferRate);
	display16("Command Classes", csd2->ccc);
	display8("Maximum Read Block Length", csd2->readBlockLength);
	displayFlag("Partial Block Reads?", csd2->readBlockPartial);
	displayFlag("Write Block Misalignment?", csd2->writeBlockMisalign);
	displayFlag("Read Block Misalignment?", csd2->readBlockMisalign);
	display8("DSR Implemented", csd2->dsr);
	display32("Device Size (Block Count)", csd2->deviceSize);
	displayFlag("Erase Block Enabled?", csd2->eraseBlockEnable);
	display8("Erase Sector Size", csd2->eraseSectorSize);
	display8("Write Protect Group Size", csd2->wpGroupSize);
	displayFlag("Write Protect Group Enabled?", csd2->wpGroupEnable);
	display8("Write Speed Factor", csd2->writeSpeedFactor);
	display8("Max Write Block Length", csd2->writeBlockLength);
	displayFlag("Partial Block Writes?", csd2->writeBlockPartial);
	display8("File Format Group", csd2->fileFormatGroup);
	displayFlag("Copy?", csd2->copy);
	displayFlag("Permanent Write Protection?", csd2->wpPermanent);
	displayFlag("Temporary Write Protection?", csd2->wpTemporary);
	display8("File Format", csd2->fileFormat);
	display8("CSD Checksum", csd2->checksum);
	putchar('\n');
}

static void dumpCID(struct CID *cid)
{
	display8("Manufacturer", cid->manufacturer);
	displaySubstring("OEM/Application", cid->oem, sizeof(cid->oem));
	displaySubstring("Product", cid->product, sizeof(cid->product));
	displayVersion("Revision", cid->majorRevision, cid->minorRevision);
	display32("Serial Number", cid->serialNumber);
	display8("Reserved", cid->reserved);
	displayDate("Manufactured", cid->year, cid->month);
	display8("Checksum", cid->checksum);
	putchar('\n');
}

static void displayBlockToken(struct Block *block)
{
	char *description = "Unknown";

	switch (block->token)
	{
		case BlockError:
			description = "Error";
			break;

		case BlockCCError:
			description = "CC Error";
			break;

		case BlockECCFailure:
			description = "Card ECC Failure";
			break;

		case BlockOutOfRange:
			description = "Out of Range";
			break;

		case BlockStart:
			description = "Block Start";
			break;
	}

	describe8("Token", block->token, description);
	putchar('\n');
}

static void displayBlockChecksum(struct Block *block)
{
	uint16_t checksum = 0;
	calculateCRC16(block->data, block->length, &checksum);
	display16("Checksum (received)", block->checksum);
	display16("Checksum (calculated)", checksum);
	putchar('\n');
}

static void dumpWriteStatus(enum WriteStatus *writeStatus)
{
	char *description = "Unknown";

	switch (*writeStatus)
	{
		case NotWritten:
			description = "Not Written";
			break;

		case WriteAccepted:
			description = "Accepted";
			break;

		case WriteCRCError:
			description = "CRC Error";
			break;

		case WriteError:
			description = "Error";
			break;

		default:
			break;
	}

	describe8("Write Status", *writeStatus, description);
	putchar('\n');
}

static void displayString(char *label, char *string)
{
	printf("  %-32s%s\n", label, string);
}

static void displaySubstring(char *label, char *string, size_t length)
{
	printf("  %-32s", label);
	fwrite(string, length, 1, stdout);
	putchar('\n');
}

static void displayDate(char *label, uint16_t year, uint16_t month)
{
	printf("  %-32s20%02d/%02d\n", label, year, month);
}

static void displayVersion(char *label, uint8_t major, uint8_t minor)
{
	printf("  %-32s%d.%d\n", label, major, minor);
}

static void displayFlag(char *label, uint8_t value)
{
	printf("  %-32s0x%02x (%s)\n", label, value, value ? "Yes": "No");
}

static void displayFrequency(char *label, uint32_t value)
{
	printf("  %-32s%dHz\n", label, value);
}

static void displayMiliseconds(char *label, uint32_t value)
{
	printf("  %-32s%dms\n", label, value);
}

static void display8(char *label, uint8_t value)
{
	printf("  %-32s0x%02x\n", label, value);
}

static void describe8(char *label, uint8_t value, char *description)
{
	printf("  %-32s0x%02x (%s)\n", label, value, description);
}

static void display16(char *label, uint16_t value)
{
	printf("  %-32s0x%04x\n", label, value);
}

static void display32(char *label, uint32_t value)
{
	printf("  %-32s0x%08x\n", label, value);
}

static void describe32(char *label, uint32_t value, char *description)
{
	printf("  %-32s0x%08x (%s)\n", label, value, description);
}

static void dump(uint8_t *buffer, size_t length, FILE *stream)
{
	for (int offset = 0; offset < length; offset += 16)
	{
		int remaining = 16;

		if (offset + 16 >= length)
		{
			remaining = length - offset;
		}

		fprintf(stream, "  %08x: ", offset);

		for (int index = 0; index < 16; index++)
		{
			if (index < remaining)
			{
				uint8_t byte = buffer[offset + index];
				fprintf(stream, "%02x", byte);
			}

			else
			{
				fprintf(stream, "  ");
			}

			if (index % 2)
			{
				fputc(' ', stream);
			}
		}

		fputc(' ', stream);

		for (int index = 0; index < remaining; index++)
		{
			uint8_t byte = buffer[offset + index];
			fputc(isprint(byte) ? byte : '.', stream);
		}

		fputc('\n', stream);
	}

	fputc('\n', stream);
}

static int parseUInt16(char **cursor, uint16_t *destination)
{
	unsigned long integer = 0;

	while (isspace(**cursor))
	{
		(*cursor)++;
	}

	if (**cursor == 0)
	{
		return -1;
	}

	errno = 0;
	integer = strtoul(*cursor, cursor, 0);

	if (errno)
	{
		return -1;
	}

	if (!isspace(**cursor) && **cursor != 0)
	{
		return -1;
	}

	*destination = integer;
	return 0;
}

static uint32_t slice(uint8_t *data, int offset, int length)
{
        uint32_t slice = 0;

        uint8_t  bitOffset  = offset % 8;
        uint8_t  byteOffset = offset / 8;
        uint8_t *cursor     = data + byteOffset;
        uint8_t  shift      = 0;

        while (length > 0)
        {
                if (bitOffset > 7)
                {
                        bitOffset = 0;
                        cursor++;
                }

                shift = 7 - bitOffset;
                slice = (slice << 1) | ((*cursor >> shift) & 1);

                bitOffset++;
                length--;
        }

        return slice;
}

static int parseUInt32(char **cursor, uint32_t *destination)
{
	unsigned long integer = 0;

	while (isspace(**cursor))
	{
		(*cursor)++;
	}

	if (**cursor == 0)
	{
		return -1;
	}

	errno = 0;
	integer = strtoul(*cursor, cursor, 0);

	if (errno)
	{
		return -1;
	}

	if (!isspace(**cursor) && **cursor != 0)
	{
		return -1;
	}

	*destination = integer;
	return 0;
}

static int parseFilename(char **cursor, char **filename)
{
	while (isspace(**cursor))
	{
		(*cursor)++;
	}

	if (**cursor == 0)
	{
		return -1;
	}

	*filename = *cursor;

	while (!isspace(**cursor))
	{
		if (**cursor == 0)
		{
			break;
		}

		(*cursor)++;
	}

	*(*cursor)++ = 0;
	return 0;
}
