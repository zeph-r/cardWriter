#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <xpcsc.hpp>

using namespace std;

typedef enum { Key_None, Key_A, Key_B } CardBlockKeyType;
struct AccessBits {
	xpcsc::Byte C1 : 1, C2 : 1, C3 : 1;
	bool is_set;
	AccessBits()
		: is_set(false) {}
	;
};
struct CardContents {
	xpcsc::Byte blocks_data[64][16];
	xpcsc::Byte blocks_keys[64][6];

	CardBlockKeyType blocks_key_types[64];
	AccessBits blocks_access_bits[64];
};

const uint8_t CARD_SECTOR  = 0x5;
const uint8_t CARD_SECTOR_BLOCK = 0x2;
const uint8_t CARD_BLOCK = ((CARD_SECTOR * 4) + CARD_SECTOR_BLOCK);
const uint8_t CARD_SECTOR_TRAILER = ((CARD_SECTOR * 4) + 3);
const uint8_t DEFAULT_KEY_A[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const uint16_t INITIAL_BALANCE = 15000;
const uint16_t TICKET_PRICE = 170;

// template for Load Keys command
const xpcsc::Byte CMD_LOAD_KEYS[] = {
	 0xFF,
	0x82,
	0x00,
	0x00, 
	0x06,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 
};

// template for General Auth command
const xpcsc::Byte CMD_GENERAL_AUTH[] = {
	 0xFF,
	0x86,
	0x00,
	0x00, 
	0x05,
	0x01,
	0x00,
	0x00,
	0x60,
	0x00
 };

// template for Read Binary command
const xpcsc::Byte CMD_READ_BINARY[] = { 0xFF, 0xB0, 0x00, 0x00, 0x10 };

// template for Update Binary command
unsigned char CMD_UPDATE_BINARY[] = { 
	0xFF,
	0xD6,
	0x00,
	0x00,
	0x10,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 
};

xpcsc::Connection c;

xpcsc::Reader reader;

void init_reader()
{

	try {
		c.init();
	}
	catch (xpcsc::PCSCError &e) {
		std::cerr << "Connection to PC/SC failed: " << e.what() << std::endl;
		return;
	}
	
	std::vector<std::string> readers = c.readers();
	if (readers.size() == 0) {
		std::cerr << "[E] No connected readers" << std::endl;
		return;
	}

	try {
		std::string reader_name = *readers.begin();
		std::cout << "Found reader: " << reader_name << std::endl;
		reader = c.wait_for_reader_card(reader_name);
	}
	catch (xpcsc::PCSCError &e) {
		std::cerr << "Wait for card failed: " << e.what() << std::endl;
		return;
	}
	
	xpcsc::Bytes atr = c.atr(reader);
	std::cout << "ATR: " << xpcsc::format(atr) << std::endl;

	// parse ATR
	xpcsc::ATRParser p;
	p.load(atr);
	
	std::cout << p.str() << std::endl;
	
}

void activate()
{
	
}

void read()
{
	xpcsc::Bytes command;
	xpcsc::Bytes response;
	// загружаем ACTIVE_KEY_A
	command.assign(CMD_LOAD_KEYS, sizeof(CMD_LOAD_KEYS));
	command.replace(5, 6, DEFAULT_KEY_A, 6);
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Failed to load key" << std::endl;
		return;
	}

	// аутентифицируемся ключом A, чтобы получить доступ к блоку CARD_BLOCK 
	command.assign(CMD_GENERAL_AUTH, sizeof(CMD_GENERAL_AUTH));
	command[7] = CARD_BLOCK;
	command[8] = 0x60;
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Cannot authenticate using ACTIVE_KEY_A!" << std::endl;
	}

	// читаем CARD_BLOCK
	command.assign(CMD_READ_BINARY, sizeof(CMD_READ_BINARY));
	command[3] = CARD_BLOCK;
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Cannot read block!" << std::endl;
		return;
	}

	// извлекаем баланс из первых двух байтов
	uint16_t balance = 0;
	memcpy(&balance, response.c_str(), 2);

	// печатаем баланс
	std::cout << "Card id is: " << balance << std::endl;
}

void write(int id)
{
	xpcsc::Bytes command;
	xpcsc::Bytes response;
	
	// загружаем ACTIVE_KEY_A
	command.assign(CMD_LOAD_KEYS, sizeof(CMD_LOAD_KEYS));
	command.replace(5, 6, DEFAULT_KEY_A, 6);
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Failed to load key" << std::endl;
		return;
	}

	// аутентифицируемся ключом A, чтобы получить доступ к блоку CARD_BLOCK 
	command.assign(CMD_GENERAL_AUTH, sizeof(CMD_GENERAL_AUTH));
	command[7] = CARD_BLOCK;
	command[8] = 0x60;
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Cannot authenticate using ACTIVE_KEY_A!" << std::endl;
	}

	
	// создаём пустой блок с нулями
	xpcsc::Bytes balance_block(16, 0);
	// записываем баланс в первые два байта
	balance_block.replace(0, 2, (xpcsc::Byte*)&id, 2);

	// записываем весь блок на карту
	command.assign(CMD_UPDATE_BINARY, sizeof(CMD_UPDATE_BINARY));
	command[3] = CARD_BLOCK;
	// копируем 16 байтов блока в команду
	command.replace(5, 16, balance_block.c_str(), 16);
	c.transmit(reader, command, &response);
	if (c.response_status(response) != 0x9000) {
		std::cerr << "Cannot update block!" << std::endl;
		return;
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2 || argc > 3)
	{
		std::cout << "wrong number of options\n";
		return 1;
	}

	option opts[] =
	{ 
		{ "read", no_argument, 0, 'r' },
		{ "write", required_argument, 0, 'w' },
	};
	
	int optindex = 0;
	
	char opchar = getopt_long(argc, argv, "rw:", opts, &optindex);
	
	switch (opchar)
	{
	case 'r':
		init_reader();
		std::cout << "Reading...\n";
		read();
		break;
	case 'w':
		init_reader();
		int id = atoi(optarg);
		std::cout << "Writing... " << id << std::endl;
		write(id);
		std::cout << "Success ! " << id << std::endl;
	}
	return 0;
}