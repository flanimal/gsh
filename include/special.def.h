#define WHITESPACE_CHARS ' ', '\f', '\n', '\r', '\t', '\v',
#define WHITESPACE               \
	(char[])                 \
	{                        \
		WHITESPACE_CHARS \
	}

#define AS_ENUM(name, ch) GSH_##name = ch,
#define AS_ARRAY(name, ch) ch,

/*
 *	Special characters.
 */
#define SPECIAL_CHARS(X)    \
	X(SINGLE_QUO, '\'') \
	X(DOUBLE_QUO, '\"') \
	X(OPEN_PAREN, '(')  \
	X(CLOSE_PAREN, ')') \
	X(REF_HOME, '~')    \
	X(REF_PARAM, '$')   \
	X(CMD_SEP, ';')

enum gsh_token_type { GSH_WORD, SPECIAL_CHARS(AS_ENUM) };

static const char gsh_special_chars[] = {
	SPECIAL_CHARS(AS_ARRAY) WHITESPACE_CHARS '\0'
};

#undef AS_ENUM
#undef AS_ARRAY

#undef SPECIAL_CHARS