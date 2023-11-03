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

/*
 *	Special parameters.
 */
#define SPECIAL_PARAMS(X) X(PARAM_STATUS, '?')

#define CHAR_ENUM(name, ch) GSH_##name = ch,
#define CHAR_ARRAY(name, ch) ch,

enum gsh_special_char { GSH_WORD, SPECIAL_CHARS(CHAR_ENUM) };
enum gsh_special_param { SPECIAL_PARAMS(CHAR_ENUM) };

static const char gsh_special_chars[] = {
	SPECIAL_CHARS(CHAR_ARRAY) ' ', '\n', '\r', '\t', '\v', '\0'
};

#undef CHAR_ENUM
#undef CHAR_ARRAY

#undef SPECIAL_CHARS
#undef SPECIAL_PARAMS