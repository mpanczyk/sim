/*	This file is part of the software similarity tester SIM.
	Written by Dick Grune, Vrije Universiteit, Amsterdam.
	$Id: tokenarray.h,v 1.6 2015-01-12 09:16:13 dick Exp $
*/

/* Interface for the token storage */
extern void Init_Token_Array(void);
extern void Store_Token(Token tk);
extern size_t Token_Array_Length(void);	/* also first free token position */
extern Token *Token_Array;

