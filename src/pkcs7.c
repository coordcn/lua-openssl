/*=========================================================================*\
* pkcs7.c
* PKCS7 module for lua-openssl binding
*
* Author:  george zhao <zhaozg(at)gmail.com>
\*=========================================================================*/

#include "openssl.h"

#define MYNAME		"pkcs7"
#define MYVERSION	MYNAME " library for " LUA_VERSION " / Nov 2014 / "\
	"based on OpenSSL " SHLIB_VERSION_NUMBER
#define MYTYPE			"openssl.pkcs7"

static BIO* load_bio_object(lua_State* L, int idx) {
	BIO* bio = NULL;
	if(lua_isstring(L, idx))
	{
		size_t l = 0;
		const char* ctx = lua_tolstring(L, idx, &l);
		bio = BIO_new_mem_buf((void*)ctx, l);
	}else if(auxiliar_isclass(L,"openssl.bio", idx))
	{
		bio = CHECK_OBJECT(idx,BIO, "openssl.bio");
		bio->references++;
	}else
		luaL_argerror(L, idx, "only support string or openssl.bio");
	return bio;
}
enum {
	FORMAT_AUTO = 0,
	FORMAT_DER,
	FORMAT_PEM,
	FORMAT_SMIME,
	FORMAT_NUM
};

static const char* format[] = {
	"auto",
	"der",
	"pem",
	"smime",
	NULL
};

static LUA_FUNCTION(openssl_pkcs7_read) {
	BIO* bio = load_bio_object(L, 1);
	int fmt = luaL_checkoption(L, 2, "auto", format);
	PKCS7 *p7 = NULL;
	BIO* ctx = NULL;

	if(fmt==FORMAT_AUTO || fmt==FORMAT_DER){
		p7 = d2i_PKCS7_bio(bio,NULL);
		BIO_reset(bio);
	}
	if((fmt==FORMAT_AUTO && p7==NULL)||fmt==FORMAT_PEM)
	{
		p7 = PEM_read_bio_PKCS7(bio,NULL,NULL,NULL);
		BIO_reset(bio);
	}
	if((fmt==FORMAT_AUTO && p7==NULL)||fmt==FORMAT_SMIME)
	{
		p7 = SMIME_read_PKCS7(bio,&ctx);
	}

	BIO_free(bio);
	if(p7)
	{
		PUSH_OBJECT(p7,"openssl.pkcs7");
		if(ctx){
			BUF_MEM* mem;
			BIO_get_mem_ptr(ctx, &mem);
			lua_pushlstring(L,mem->data, mem->length);
			BIO_free(ctx);
			return 2;
		}
	}
	else
		lua_pushnil(L);

	return 1;
}

static LUA_FUNCTION(openssl_pkcs7_sign)
{
	X509 * cert = NULL;
	EVP_PKEY * privkey = NULL;
	long flags = 0;//PKCS7_DETACHED;
	PKCS7 * p7 = NULL;
	BIO * in = NULL;
	STACK_OF(X509) *others = NULL;


	int top = lua_gettop(L);
	int ret = 0;

	in = load_bio_object(L, 1);
	cert = CHECK_OBJECT(2,X509,"openssl.x509");
	privkey = CHECK_OBJECT(3, EVP_PKEY,"openssl.evp_pkey");
	if(top>3)
		flags = luaL_checkint(L,4);
	if(top>4)
		others = CHECK_OBJECT(5, STACK_OF(X509), "openssl.stack_of_x509");

	p7 = PKCS7_sign(cert, privkey, others, in, flags);
	if (p7 == NULL) {
		luaL_error(L,"error creating PKCS7 structure!");
	}

	BIO_free(in);

	if(p7) {
		PUSH_OBJECT(p7,"openssl.pkcs7");
		return 1;
	}else
		luaL_error(L,"error creating PKCS7 structure!");

	return 0;
}

static LUA_FUNCTION(openssl_pkcs7_verify)
{
	X509_STORE * store = NULL;
	STACK_OF(X509) *signers = NULL;
	STACK_OF(X509) *cainfo = NULL;
	STACK_OF(X509) *others = NULL;
	PKCS7 * p7 = NULL;
	BIO* dataout = NULL;
	long flags = 0;

	int ret = 0;
	int top = lua_gettop(L);

	p7 = CHECK_OBJECT(1,PKCS7,"openssl.pkcs7");
	if(top>1)
		flags = luaL_checkinteger(L,2);
	if(top>2)
		signers = CHECK_OBJECT(3, STACK_OF(X509),"openssl.stack_of_x509");
	if(top>3)
		cainfo = CHECK_OBJECT(4, STACK_OF(X509),"openssl.stack_of_x509");
	if(top>4)
		others = CHECK_OBJECT(5, STACK_OF(X509),"openssl.stack_of_x509");

	if(top>5)
		dataout = CHECK_OBJECT(6, BIO, "openssl.bio");

	flags = flags & ~PKCS7_DETACHED;
	store = setup_verify(cainfo);

	if (!store) {
		luaL_error(L, "can't setup veirfy cainfo");
	}

	if (PKCS7_verify(p7, others, store, NULL, dataout, flags)) {
		STACK_OF(X509) *signers1 = PKCS7_get0_signers(p7, NULL, flags);
		ret = 2;
		lua_pushboolean(L,1);
		PUSH_OBJECT(signers1,"openssl.sk_x509");
	} else {
		lua_pushboolean(L,0);
		ret = 1;
	}
	if(store)
		X509_STORE_free(store);
	if(dataout)
		BIO_free(dataout);

	return 1;
}

static LUA_FUNCTION(openssl_pkcs7_encrypt)
{
	STACK_OF(X509) * recipcerts = NULL;
	BIO * infile = NULL;
	long flags = 0;
	PKCS7 * p7 = NULL;
	const EVP_CIPHER *cipher = EVP_get_cipherbynid(OPENSSL_CIPHER_DEFAULT);
	int ret = 0;
	int top = lua_gettop(L);

	infile = load_bio_object(L, 1);
	recipcerts = CHECK_OBJECT(2,STACK_OF(X509),"openssl.stack_of_x509");
	if (top>2)
		flags = luaL_checkinteger(L,3);
	if(top>3)
	{
		cipher = CHECK_OBJECT(4,EVP_CIPHER,"openssl.evp_cipher");
	}else
		cipher = EVP_get_cipherbyname("DES-EDE-CBC");

	/* sanity check the cipher */
	if (cipher == NULL) {
		/* shouldn't happen */
		luaL_error(L, "Failed to get cipher");
	}

	p7 = PKCS7_encrypt(recipcerts, infile, (EVP_CIPHER*)cipher, flags);
	BIO_reset(infile);

	if (p7 == NULL) {
		lua_pushnil(L);
	}else {
		PUSH_OBJECT(p7,"openssl.pkcs7");
	}
	return 1;
}

static LUA_FUNCTION(openssl_pkcs7_decrypt)
{
	X509 * cert = NULL;
	EVP_PKEY * key = NULL;

	BIO * out = NULL, * datain = NULL;
	PKCS7 * p7 = NULL;

	int ret = 0;

	p7 = CHECK_OBJECT(1, PKCS7, "openssl.pkcs7");
	cert = CHECK_OBJECT(2,X509,"openssl.x509");
	key = lua_isnoneornil(L,3)?NULL: CHECK_OBJECT(3,EVP_PKEY,"openssl.evp_pkey");
	out = BIO_new(BIO_s_mem());

	if (PKCS7_decrypt(p7, key, cert, out, PKCS7_DETACHED)) {
		BUF_MEM* mem;
		BIO_get_mem_ptr(out, &mem);
		lua_pushlstring(L,mem->data, mem->length);
	}else
		lua_pushnil(L);
	BIO_free(out);
	return 1;
}

/*** pkcs7 object method ***/
static LUA_FUNCTION(openssl_pkcs7_gc) {
    PKCS7* p7 = CHECK_OBJECT(1,PKCS7,"openssl.pkcs7");
    PKCS7_free(p7);
    return 0;
}

static LUA_FUNCTION(openssl_pkcs7_export)
{
    int pem;
    PKCS7 * p7 = CHECK_OBJECT(1,PKCS7,"openssl.pkcs7");
    int top = lua_gettop(L);
    BIO* bio_out = NULL;

    pem = top > 1 ? lua_toboolean(L, 2) : 1;

    bio_out	 = BIO_new(BIO_s_mem());
    if (pem) {

        if (PEM_write_bio_PKCS7(bio_out, p7))  {
            BUF_MEM *bio_buf;
            BIO_get_mem_ptr(bio_out, &bio_buf);
            lua_pushlstring(L,bio_buf->data, bio_buf->length);
        } else
            lua_pushnil(L);
    } else
    {
        if(i2d_PKCS7_bio(bio_out, p7)) {
            BUF_MEM *bio_buf;
            BIO_get_mem_ptr(bio_out, &bio_buf);
            lua_pushlstring(L,bio_buf->data, bio_buf->length);
        } else
            lua_pushnil(L);
    }

    BIO_free(bio_out);
    return 1;
}

static int PKCS7_type_is_other(PKCS7* p7)
{
    int isOther=1;

    int nid=OBJ_obj2nid(p7->type);

    switch( nid )
    {
    case NID_pkcs7_data:
    case NID_pkcs7_signed:
    case NID_pkcs7_enveloped:
    case NID_pkcs7_signedAndEnveloped:
    case NID_pkcs7_digest:
    case NID_pkcs7_encrypted:
        isOther=0;
        break;
    default:
        isOther=1;
    }

    return isOther;

}
static ASN1_OCTET_STRING *PKCS7_get_octet_string(PKCS7 *p7)
{
    if ( PKCS7_type_is_data(p7))
        return p7->d.data;
    if ( PKCS7_type_is_other(p7) && p7->d.other
            && (p7->d.other->type == V_ASN1_OCTET_STRING))
        return p7->d.other->value.octet_string;
    return NULL;
}

/*
int openssl_signerinfo_parse(lua_State*L)
{
	PKCS7_SIGNER_INFO * si = CHECK_OBJECT(1,PKCS7_SIGNER_INFO,"openssl.pkcs7_signer_info");
	si->

}
*/
static LUA_FUNCTION(openssl_pkcs7_parse)
{
    PKCS7 * p7 = CHECK_OBJECT(1,PKCS7,"openssl.pkcs7");
    STACK_OF(X509) *certs=NULL;
    STACK_OF(X509_CRL) *crls=NULL;
    int i=OBJ_obj2nid(p7->type);

    lua_newtable(L);
    lua_pushstring(L,OBJ_nid2ln(i));
    lua_setfield(L,-2,"type");

    switch (i)
    {
    case NID_pkcs7_signed:
    {
        PKCS7_SIGNED *sign = p7->d.sign;
		PKCS7* c = sign->contents;
		PKCS7_SIGNER_INFO* si = sk_PKCS7_SIGNER_INFO_value(sign->signer_info,0);
		(void*)si;
        certs = sign->cert? sign->cert : NULL;
        crls = sign->crl ? sign->crl : NULL;
#if 0

		typedef struct pkcs7_signed_st
		{
			ASN1_INTEGER			*version;	/* version 1 */
			STACK_OF(X509_ALGOR)		*md_algs;	/* md used */
			STACK_OF(X509)			*cert;		/* [ 0 ] */
			STACK_OF(X509_CRL)		*crl;		/* [ 1 ] */
			STACK_OF(PKCS7_SIGNER_INFO)	*signer_info;

			struct pkcs7_st			*contents;
		} PKCS7_SIGNED;
#endif
        PUSH_OBJECT(sk_X509_ALGOR_dup(sign->md_algs),"openssl.stack_of_x509_algor");
        lua_setfield(L,-2,"md_algs");
        PUSH_OBJECT(sk_PKCS7_SIGNER_INFO_dup(sign->signer_info),"openssl.stack_of_pkcs7_signer_info");
        lua_setfield(L,-2,"signer_info");
        lua_pushboolean(L,PKCS7_is_detached(p7));
        lua_setfield(L,-2,"detached");
		if(c){
			PUSH_OBJECT(PKCS7_dup(c), "openssl.pkcs7");
			lua_setfield(L, -2, "contents");
		}
        if(!PKCS7_is_detached(p7)) {
#if 0
            BIO  *bio = BIO_new(BIO_s_mem());
            ADD_ASSOC_ASN1_STRING(ASN1_OCTET_STRING, bio, PKCS7_get_octet_string(p7->d.sign->contents), "content");
            BIO_free(bio);
#else
            ASN1_OCTET_STRING *os = PKCS7_get_octet_string(p7->d.sign->contents);
            lua_pushlstring(L,(const char*)os->data, os->length);
            lua_setfield(L,-2,"content");
#endif
        }
    }
    break;
    case NID_pkcs7_signedAndEnveloped:
        certs=p7->d.signed_and_enveloped->cert;
        crls=p7->d.signed_and_enveloped->crl;
        break;
	case NID_pkcs7_enveloped:
		{/*
			BIO * mem = BIO_new(BIO_s_mem());
			BIO * v_p7bio = PKCS7_dataDecode(p7,pkey,NULL,NULL);
			BUF_MEM *bptr = NULL;
			unsigned char src[4096];
			int len;

			while((len = BIO_read(v_p7bio,src,4096))>0){
				BIO_write(mem, src, len);
			}
			BIO_free(v_p7bio);
			BIO_get_mem_ptr(mem, &bptr);
			if((int)*puiDataLen < bptr->length)
			{
				*puiDataLen = bptr->length;
				ret = SAR_MemoryErr;
			}else{
				*puiDataLen =  bptr->length;
				memcpy(pucData,bptr->data, bptr->length);
			}
		*/
		}
		break;
	case NID_pkcs7_digest:
		{
			PKCS7_DIGEST* d =p7->d.digest;
			PKCS7* c = d->contents;
			ASN1_OCTET_STRING *data = d->digest;
			(void*)c;

			lua_pushliteral(L, "digest");
			lua_setfield(L, -2, "type");

			if(data){
				int dlen = ASN1_STRING_length(data);
				unsigned char* dptr = ASN1_STRING_data(data);
				lua_pushlstring(L, (const char*)dptr, dlen);
				lua_setfield(L, -2, "digest");
			}
		}
		break;
	case NID_pkcs7_data:
	{
		ASN1_OCTET_STRING *data = p7->d.data;
		int dlen = ASN1_STRING_length(data);
		unsigned char* dptr = ASN1_STRING_data(data);

		lua_pushliteral(L, "data");
		lua_setfield(L, -2, "type");
		lua_pushlstring(L, (const char*)dptr, dlen);
		lua_setfield(L, -2, "data");
	}
		break;
    default:
        break;
    }

    if (certs != NULL)
    {
        PUSH_OBJECT(sk_X509_dup(certs), "openssl.stack_of_x509");
        lua_setfield(L,-2, "certs");
    }
    if (crls != NULL)
    {
        PUSH_OBJECT(sk_X509_CRL_dup(crls), "openssl.stack_of_crl");
        lua_setfield(L,-2, "crls");
    }

    return 1;
}

#if 0
/*

#if 0
int headers = 5;
, * outfile = NULL
outfile = CHECK_OBJECT(2, BIO, "openssl.bio");
/* tack on extra headers */
/* table is in the stack at index 't' */
lua_pushnil(L);  /* first key */
while (lua_next(L, headers) != 0) {
	/* uses 'key' (at index -2) and 'value' (at index -1) */
	//printf("%s - %s\n",lua_typename(L, lua_type(L, -2)), lua_typename(L, lua_type(L, -1)));
	const char *idx = lua_tostring(L,-2);
	const char *val = luaL_checkstring(L,-1);

	BIO_printf(outfile, "%s: %s\n", idx, val);

	/* removes 'value'; keeps 'key' for next iteration */
	lua_pop(L, 1);
}

/* write the signed data */
ret = SMIME_write_PKCS7(outfile, p7, infile, flags);

/* tack on extra headers */
/* table is in the stack at index 't' */
lua_pushnil(L);  /* first key */
while (lua_next(L, headers) != 0) {
	/* uses 'key' (at index -2) and 'value' (at index -1) */
	//printf("%s - %s\n",lua_typename(L, lua_type(L, -2)), lua_typename(L, lua_type(L, -1)));
	const char *idx = lua_tostring(L,-2);
	const char *val = luaL_checkstring(L,-1);

	BIO_printf(outfile, "%s: %s\n", idx, val);

	/* removes 'value'; keeps 'key' for next iteration */
	lua_pop(L, 1);
}
*/
#endif




static luaL_Reg pkcs7_funcs[] = {
	{"parse",				openssl_pkcs7_parse},
	{"export",				openssl_pkcs7_export},
	{"decrypt",				openssl_pkcs7_decrypt},
	{"verify",				openssl_pkcs7_verify},

	{"__gc",				openssl_pkcs7_gc       },
	{"__tostring",			auxiliar_tostring },

	{NULL,			NULL}
};

static const luaL_Reg R[] =
{
	{"read",			openssl_pkcs7_read},
	{"sign",			openssl_pkcs7_sign},
	{"sign",			openssl_pkcs7_verify},
	{"encrypt",			openssl_pkcs7_encrypt},
	{"decrypt",			openssl_pkcs7_decrypt},

	{NULL,	NULL}
};

LUALIB_API int luaopen_pkcs7(lua_State *L)
{
	auxiliar_newclass(L,"openssl.pkcs7", pkcs7_funcs);

	luaL_newmetatable(L,MYTYPE);
	lua_setglobal(L,MYNAME);
	luaL_register(L,MYNAME,R);
	lua_pushvalue(L, -1);
	lua_setmetatable(L, -2);
	lua_pushliteral(L,"version");			/** version */
	lua_pushliteral(L,MYVERSION);
	lua_settable(L,-3);
	lua_pushliteral(L,"__index");
	lua_pushvalue(L,-2);
	lua_settable(L,-3);
	return 1;
}


