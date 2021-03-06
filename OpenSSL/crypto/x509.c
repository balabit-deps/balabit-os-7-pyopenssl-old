/*
 * x509.c
 *
 * Copyright (C) AB Strakt
 * Copyright (C) Jean-Paul Calderone
 * See LICENSE for details.
 *
 * Certificate (X.509) handling code, mostly thin wrappers around OpenSSL.
 * See the file RATIONALE for a short explanation of why this module was written.
 *
 * Reviewed 2001-07-23
 */
#include <Python.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define crypto_MODULE
#include "crypto.h"
#include "x509ext.h"

/*
 * X.509 is a standard for digital certificates.  See e.g. the OpenSSL homepage
 * http://www.openssl.org/ for more information
 */

static char crypto_X509_get_version_doc[] = "\n\
Return version number of the certificate\n\
\n\
@return: Version number as a Python integer\n\
";

static PyObject *
crypto_X509_get_version(crypto_X509Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_version"))
        return NULL;

    return PyLong_FromLong((long)X509_get_version(self->x509));
}

static char crypto_X509_set_version_doc[] = "\n\
Set version number of the certificate\n\
\n\
@param version: The version number\n\
@return: None\n\
";

static PyObject *
crypto_X509_set_version(crypto_X509Obj *self, PyObject *args)
{
    int version;

    if (!PyArg_ParseTuple(args, "i:set_version", &version))
        return NULL;

    X509_set_version(self->x509, version);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_serial_number_doc[] = "\n\
Return serial number of the certificate\n\
\n\
@return: Serial number as a Python integer\n\
";

static PyObject *
crypto_X509_get_serial_number(crypto_X509Obj *self, PyObject *args)
{
    ASN1_INTEGER *asn1_i;
    BIGNUM *bignum;
    char *hex;
    PyObject *res;

    if (!PyArg_ParseTuple(args, ":get_serial_number"))
        return NULL;

    asn1_i = X509_get_serialNumber(self->x509);
    bignum = ASN1_INTEGER_to_BN(asn1_i, NULL);
    hex = BN_bn2hex(bignum);
    res = PyLong_FromString(hex, NULL, 16);
    BN_free(bignum);
    free(hex);
    return res;
}

static char crypto_X509_set_serial_number_doc[] = "\n\
Set serial number of the certificate\n\
\n\
@param serial: The serial number\n\
@return: None\n\
";

static PyObject *
crypto_X509_set_serial_number(crypto_X509Obj *self, PyObject *args)
{
    long small_serial;
    PyObject *serial = NULL;
    PyObject *hex = NULL;
    ASN1_INTEGER *asn1_i = NULL;
    BIGNUM *bignum = NULL;
    char *hexstr;

    if (!PyArg_ParseTuple(args, "O:set_serial_number", &serial)) {
        return NULL;
    }

    if (!PyOpenSSL_Integer_Check(serial)) {
        PyErr_SetString(
            PyExc_TypeError, "serial number must be integer");
        goto err;
    }

    if ((hex = PyOpenSSL_LongToHex(serial)) == NULL) {
        goto err;
    }

#ifdef PY3
    {
        PyObject *hexbytes = PyUnicode_AsASCIIString(hex);
        Py_DECREF(hex);
        hex = hexbytes;
    }
#endif

    /**
     * BN_hex2bn stores the result in &bignum.  Unless it doesn't feel like
     * it.  If bignum is still NULL after this call, then the return value
     * is actually the result.  I hope.  -exarkun
     */
    hexstr = PyBytes_AsString(hex);
    if (hexstr[1] == 'x') {
        /* +2 to skip the "0x" */
        hexstr += 2;
    }
    small_serial = BN_hex2bn(&bignum, hexstr);

    Py_DECREF(hex);
    hex = NULL;

    if (bignum == NULL) {
        if (ASN1_INTEGER_set(X509_get_serialNumber(self->x509), small_serial)) {
            exception_from_error_queue(crypto_Error);
            goto err;
        }
    } else {
        asn1_i = BN_to_ASN1_INTEGER(bignum, NULL);
        BN_free(bignum);
        bignum = NULL;
        if (asn1_i == NULL) {
            exception_from_error_queue(crypto_Error);
            goto err;
        }
        if (!X509_set_serialNumber(self->x509, asn1_i)) {
            exception_from_error_queue(crypto_Error);
            goto err;
        }
        ASN1_INTEGER_free(asn1_i);
        asn1_i = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;

  err:
    if (hex) {
        Py_DECREF(hex);
    }
    if (bignum) {
        BN_free(bignum);
    }
    if (asn1_i) {
        ASN1_INTEGER_free(asn1_i);
    }
    return NULL;
}

static char crypto_X509_get_issuer_doc[] = "\n\
Create an X509Name object for the issuer of the certificate\n\
\n\
@return: An X509Name object\n\
";

static PyObject *
crypto_X509_get_issuer(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *pyname;
    X509_NAME *name;

    if (!PyArg_ParseTuple(args, ":get_issuer"))
        return NULL;

    name = X509_get_issuer_name(self->x509);
    pyname = crypto_X509Name_New(name, 0);
    if (pyname != NULL)
    {
        pyname->parent_cert = (PyObject *)self;
        Py_INCREF(self);
    }
    return (PyObject *)pyname;
}

static char crypto_X509_set_issuer_doc[] = "\n\
Set the issuer of the certificate\n\
\n\
@param issuer: The issuer name\n\
@type issuer: L{X509Name}\n\
@return: None\n\
";

static PyObject *
crypto_X509_set_issuer(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *issuer;

    if (!PyArg_ParseTuple(args, "O!:set_issuer", &crypto_X509Name_Type,
			  &issuer))
        return NULL;

    if (!X509_set_issuer_name(self->x509, issuer->x509_name))
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_subject_doc[] = "\n\
Create an X509Name object for the subject of the certificate\n\
\n\
@return: An X509Name object\n\
";

static PyObject *
crypto_X509_get_subject(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *pyname;
    X509_NAME *name;

    if (!PyArg_ParseTuple(args, ":get_subject"))
        return NULL;

    name = X509_get_subject_name(self->x509);
    pyname = crypto_X509Name_New(name, 0);
    if (pyname != NULL)
    {
        pyname->parent_cert = (PyObject *)self;
        Py_INCREF(self);
    }
    return (PyObject *)pyname;
}

static char crypto_X509_set_subject_doc[] = "\n\
Set the subject of the certificate\n\
\n\
@param subject: The subject name\n\
@type subject: L{X509Name}\n\
@return: None\n\
";

static PyObject *
crypto_X509_set_subject(crypto_X509Obj *self, PyObject *args)
{
    crypto_X509NameObj *subject;

    if (!PyArg_ParseTuple(args, "O!:set_subject", &crypto_X509Name_Type,
			  &subject))
        return NULL;

    if (!X509_set_subject_name(self->x509, subject->x509_name))
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_pubkey_doc[] = "\n\
Get the public key of the certificate\n\
\n\
@return: The public key\n\
";

static PyObject *
crypto_X509_get_pubkey(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *crypto_PKey_New(EVP_PKEY *, int);
    EVP_PKEY *pkey;
    crypto_PKeyObj *py_pkey;

    if (!PyArg_ParseTuple(args, ":get_pubkey"))
        return NULL;

    if ((pkey = X509_get_pubkey(self->x509)) == NULL)
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    py_pkey = crypto_PKey_New(pkey, 1);
    if (py_pkey != NULL) {
        py_pkey->only_public = 1;
    }
    return (PyObject *)py_pkey;
}

static char crypto_X509_set_pubkey_doc[] = "\n\
Set the public key of the certificate\n\
\n\
@param pkey: The public key\n\
@return: None\n\
";

static PyObject *
crypto_X509_set_pubkey(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *pkey;

    if (!PyArg_ParseTuple(args, "O!:set_pubkey", &crypto_PKey_Type, &pkey))
        return NULL;

    if (!X509_set_pubkey(self->x509, pkey->pkey))
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject*
_set_asn1_time(char *format, ASN1_TIME* timestamp, PyObject *args)
{
	char *when;

	if (!PyArg_ParseTuple(args, format, &when))
		return NULL;

	if (ASN1_GENERALIZEDTIME_set_string(timestamp, when) == 0) {
		ASN1_GENERALIZEDTIME dummy;
		dummy.type = V_ASN1_GENERALIZEDTIME;
		dummy.length = strlen(when);
		dummy.data = (unsigned char *)when;
		if (!ASN1_GENERALIZEDTIME_check(&dummy)) {
			PyErr_SetString(PyExc_ValueError, "Invalid string");
		} else {
			PyErr_SetString(PyExc_RuntimeError, "Unknown ASN1_GENERALIZEDTIME_set_string failure");
		}
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static char crypto_X509_set_notBefore_doc[] = "\n\
Set the time stamp for when the certificate starts being valid\n\
\n\
@param when: A string giving the timestamp, in the format:\n\
\n\
                 YYYYMMDDhhmmssZ\n\
                 YYYYMMDDhhmmss+hhmm\n\
                 YYYYMMDDhhmmss-hhmm\n\
\n\
@return: None\n\
";

static PyObject*
crypto_X509_set_notBefore(crypto_X509Obj *self, PyObject *args)
{
	return _set_asn1_time(
            BYTESTRING_FMT ":set_notBefore",
            X509_get_notBefore(self->x509), args);
}

static char crypto_X509_set_notAfter_doc[] = "\n\
Set the time stamp for when the certificate stops being valid\n\
\n\
@param when: A string giving the timestamp, in the format:\n\
\n\
                 YYYYMMDDhhmmssZ\n\
                 YYYYMMDDhhmmss+hhmm\n\
                 YYYYMMDDhhmmss-hhmm\n\
\n\
@return: None\n\
";

static PyObject*
crypto_X509_set_notAfter(crypto_X509Obj *self, PyObject *args)
{
	return _set_asn1_time(
            BYTESTRING_FMT ":set_notAfter",
            X509_get_notAfter(self->x509), args);
}

PyObject*
_get_asn1_time(char *format, ASN1_TIME* timestamp, PyObject *args)
{
	ASN1_GENERALIZEDTIME *gt_timestamp = NULL;
	PyObject *py_timestamp = NULL;

	if (!PyArg_ParseTuple(args, format)) {
		return NULL;
	}

	/*
	 * http://www.columbia.edu/~ariel/ssleay/asn1-time.html
	 */
	/*
	 * There must be a way to do this without touching timestamp->data
	 * directly. -exarkun
	 */
	if (timestamp->length == 0) {
	    Py_INCREF(Py_None);
	    return Py_None;
	} else if (timestamp->type == V_ASN1_GENERALIZEDTIME) {
		return PyBytes_FromString((char *)timestamp->data);
	} else {
		ASN1_TIME_to_generalizedtime(timestamp, &gt_timestamp);
		if (gt_timestamp == NULL) {
			exception_from_error_queue(crypto_Error);
			return NULL;
		} else {
			py_timestamp = PyBytes_FromString((char *)gt_timestamp->data);
			ASN1_GENERALIZEDTIME_free(gt_timestamp);
			return py_timestamp;
		}
	}
}

static char crypto_X509_get_notBefore_doc[] = "\n\
Retrieve the time stamp for when the certificate starts being valid\n\
\n\
@return: A string giving the timestamp, in the format:\n\
\n\
                 YYYYMMDDhhmmssZ\n\
                 YYYYMMDDhhmmss+hhmm\n\
                 YYYYMMDDhhmmss-hhmm\n\
           or None if there is no value set.\n\
";

static PyObject*
crypto_X509_get_notBefore(crypto_X509Obj *self, PyObject *args)
{
	/*
	 * X509_get_notBefore returns a borrowed reference.
	 */
	return _get_asn1_time(
		":get_notBefore", X509_get_notBefore(self->x509), args);
}


static char crypto_X509_get_notAfter_doc[] = "\n\
Retrieve the time stamp for when the certificate stops being valid\n\
\n\
@return: A string giving the timestamp, in the format:\n\
\n\
                 YYYYMMDDhhmmssZ\n\
                 YYYYMMDDhhmmss+hhmm\n\
                 YYYYMMDDhhmmss-hhmm\n\
           or None if there is no value set.\n\
";

static PyObject*
crypto_X509_get_notAfter(crypto_X509Obj *self, PyObject *args)
{
	/*
	 * X509_get_notAfter returns a borrowed reference.
	 */
	return _get_asn1_time(
		":get_notAfter", X509_get_notAfter(self->x509), args);
}


static char crypto_X509_gmtime_adj_notBefore_doc[] = "\n\
Change the timestamp for when the certificate starts being valid to the current\n\
time plus an offset.\n \
\n\
@param amount: The number of seconds by which to adjust the starting validity\n\
               time.\n\
@return: None\n\
";

static PyObject *
crypto_X509_gmtime_adj_notBefore(crypto_X509Obj *self, PyObject *args)
{
    long amount;

    if (!PyArg_ParseTuple(args, "l:gmtime_adj_notBefore", &amount))
        return NULL;

    X509_gmtime_adj(X509_get_notBefore(self->x509), amount);

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_gmtime_adj_notAfter_doc[] = "\n\
Adjust the time stamp for when the certificate stops being valid\n\
\n\
@param amount: The number of seconds by which to adjust the ending validity\n\
               time.\n\
@return: None\n\
";

static PyObject *
crypto_X509_gmtime_adj_notAfter(crypto_X509Obj *self, PyObject *args)
{
    long amount;

    if (!PyArg_ParseTuple(args, "l:gmtime_adj_notAfter", &amount))
        return NULL;

    X509_gmtime_adj(X509_get_notAfter(self->x509), amount);

    Py_INCREF(Py_None);
    return Py_None;
}


static char crypto_X509_get_signature_algorithm_doc[] = "\n\
Retrieve the signature algorithm used in the certificate\n\
\n\
@return: A byte string giving the short name of the signature algorithm used in\n\
         the certificate.\n\
@raise ValueError: If the signature algorithm is undefined.\n\
";

/* BEWARE: this function differs form the one in upstream.
   It returns the short name of the signature algorithm
   instead of the long name. This is necessary for the
   alias algorithm names to work, as only the short name
   aliases are registered in openssl. See bug #27571.
   You have been warned! */

static PyObject *
crypto_X509_get_signature_algorithm(crypto_X509Obj *self, PyObject *args) {
    int nid;

    if (!PyArg_ParseTuple(args, ":get_signature_algorithm")) {
        return NULL;
    }

    nid = X509_get_signature_nid(self->x509);
    if (nid == NID_undef) {
        PyErr_SetString(PyExc_ValueError, "Undefined signature algorithm");
        return NULL;
    }
    return PyBytes_FromString(OBJ_nid2sn(nid));
}


static char crypto_X509_sign_doc[] = "\n\
Sign the certificate using the supplied key and digest\n\
\n\
@param pkey: The key to sign with\n\
@param digest: The message digest to use\n\
@return: None\n\
";

static PyObject *
crypto_X509_sign(crypto_X509Obj *self, PyObject *args)
{
    crypto_PKeyObj *pkey;
    char *digest_name;
    const EVP_MD *digest;

    if (!PyArg_ParseTuple(args, "O!s:sign", &crypto_PKey_Type, &pkey,
			  &digest_name))
        return NULL;

    if (pkey->only_public) {
	PyErr_SetString(PyExc_ValueError, "Key has only public part");
	return NULL;
    }

    if (!pkey->initialized) {
	PyErr_SetString(PyExc_ValueError, "Key is uninitialized");
	return NULL;
    }

    if ((digest = EVP_get_digestbyname(digest_name)) == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "No such digest method");
        return NULL;
    }

    if (!X509_sign(self->x509, pkey->pkey, digest))
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_has_expired_doc[] = "\n\
Check whether the certificate has expired.\n\
\n\
@return: True if the certificate has expired, false otherwise\n\
";

static PyObject *
crypto_X509_has_expired(crypto_X509Obj *self, PyObject *args)
{
    time_t tnow;

    if (!PyArg_ParseTuple(args, ":has_expired"))
        return NULL;

    tnow = time(NULL);
    if (ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(self->x509), tnow) < 0)
        return PyLong_FromLong(1L);
    else
        return PyLong_FromLong(0L);
}

static char crypto_X509_subject_name_hash_doc[] = "\n\
Return the hash of the X509 subject.\n\
\n\
@return: The hash of the subject\n\
";

static PyObject *
crypto_X509_subject_name_hash(crypto_X509Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":subject_name_hash"))
        return NULL;

    return PyLong_FromLongLong(X509_subject_name_hash(self->x509));
}

static char crypto_X509_digest_doc[] = "\n\
Return the digest of the X509 object.\n\
\n\
@return: The digest of the object\n\
";

static PyObject *
crypto_X509_digest(crypto_X509Obj *self, PyObject *args)
{
    unsigned char fp[EVP_MAX_MD_SIZE];
    char *tmp;
    char *digest_name;
    unsigned int len,i;
    PyObject *ret;
    const EVP_MD *digest;

    if (!PyArg_ParseTuple(args, "s:digest", &digest_name))
        return NULL;

    if ((digest = EVP_get_digestbyname(digest_name)) == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "No such digest method");
        return NULL;
    }

    if (!X509_digest(self->x509,digest,fp,&len))
    {
        exception_from_error_queue(crypto_Error);
    }
    tmp = malloc(3*len+1);
    memset(tmp, 0, 3*len+1);
    for (i = 0; i < len; i++) {
        sprintf(tmp+i*3,"%02X:",fp[i]);
    }
    tmp[3*len-1] = 0;
    ret = PyBytes_FromStringAndSize(tmp,3*len-1);
    free(tmp);
    return ret;
}


static char crypto_X509_add_extensions_doc[] = "\n\
Add extensions to the certificate.\n\
\n\
@param extensions: a sequence of X509Extension objects\n\
@return: None\n\
";

static PyObject *
crypto_X509_add_extensions(crypto_X509Obj *self, PyObject *args)
{   
    PyObject *extensions, *seq;
    crypto_X509ExtensionObj *ext;
    int nr_of_extensions, i;

    if (!PyArg_ParseTuple(args, "O:add_extensions", &extensions))
        return NULL;

    seq = PySequence_Fast(extensions, "Expected a sequence");
    if (seq == NULL)
        return NULL;

    nr_of_extensions = PySequence_Fast_GET_SIZE(seq);

    for (i = 0; i < nr_of_extensions; i++)
    { 
        ext = (crypto_X509ExtensionObj *)PySequence_Fast_GET_ITEM(seq, i);
        if (!crypto_X509Extension_Check(ext))
        {   
            Py_DECREF(seq);
            PyErr_SetString(PyExc_ValueError,
                            "One of the elements is not an X509Extension");
            return NULL;
        }
        if (!X509_add_ext(self->x509, ext->x509_extension, -1))
        {
            Py_DECREF(seq);
            exception_from_error_queue(crypto_Error);
            return NULL;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static char crypto_X509_get_extension_count_doc[] = "\n\
Get the number of extensions on the certificate.\n\
\n\
@return: Number of extensions as a Python integer\n\
";

static PyObject *
crypto_X509_get_extension_count(crypto_X509Obj *self, PyObject *args) {
    if (!PyArg_ParseTuple(args, ":get_extension_count")) {
        return NULL;
    }

    return PyLong_FromLong((long)X509_get_ext_count(self->x509));
}

static char crypto_X509_get_extension_doc[] = "\n\
Get a specific extension of the certificate by index.\n\
\n\
@param index: The index of the extension to retrieve.\n\
@return: The X509Extension object at the specified index.\n\
";

static PyObject *
crypto_X509_get_extension(crypto_X509Obj *self, PyObject *args) {
    crypto_X509ExtensionObj *extobj;
    int loc;
    X509_EXTENSION *ext;

    if (!PyArg_ParseTuple(args, "i:get_extension", &loc)) {
        return NULL;
    }

    /* will return NULL if loc is outside the range of extensions,
       not registered as an error*/
    ext = X509_get_ext(self->x509, loc);
    if (!ext) {
        PyErr_SetString(PyExc_IndexError, "extension index out of bounds");
        return NULL; /* Should be reported as an IndexError ? */
    }

    extobj = PyObject_New(crypto_X509ExtensionObj, &crypto_X509Extension_Type);
    extobj->x509_extension = X509_EXTENSION_dup(ext);
    extobj->dealloc = 1;

    return (PyObject*)extobj;
}


static char crypto_X509_get_subject_alt_name_doc[] = "\n\
Return the contents of the subjectAltName extension.\n\
\n\
Arguments: self - The X509 object\n\
           args - The Python argument tuple, should be empty\n\
Returns:   A list of (int type, str altname) pairs (tuples)\n\
           or None if there's no subjectAltName extension.\n\
";

static PyObject *
crypto_X509_get_subject_alt_name(crypto_X509Obj *self, PyObject *args)
{
    X509 *cert = self->x509;
    int ext_ndx;
    char *buf;

    ext_ndx = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
    if (ext_ndx >= 0)
    {
        /* subjectAltName extension found */
        X509_EXTENSION *ext;
        STACK_OF(GENERAL_NAME) *alt_names;
        GENERAL_NAME *gen_name;

        ext = X509_get_ext(cert, ext_ndx);
        alt_names = X509V3_EXT_d2i(ext);
        if (alt_names)
        {
            int num;
            int i;
            PyObject *result;

            num = sk_GENERAL_NAME_num(alt_names);
            result = PyList_New(0);

            for (i = 0; i < num; i++)
            {
                PyObject *tuple;
                PyObject *pybuf;
                char *name;
                size_t name_len;
                int copy_name = 0;    /* set to 1 if name should be copied into buf */

                gen_name = sk_GENERAL_NAME_value(alt_names, i);

                /* allocate buf and put a string representation of the name into it */
                switch (gen_name->type)
                {
                    case GEN_EMAIL:
                        name = (char *)ASN1_STRING_get0_data(gen_name->d.rfc822Name);
                        name_len = ASN1_STRING_length(gen_name->d.rfc822Name);
                        copy_name = 1;
                        break;

                    case GEN_DNS:
                        name = (char *)ASN1_STRING_get0_data(gen_name->d.dNSName);
                        name_len = ASN1_STRING_length(gen_name->d.dNSName);
                        copy_name = 1;
                        break;

                    case GEN_URI:
                        name = (char *)ASN1_STRING_get0_data(gen_name->d.uniformResourceIdentifier);
                        name_len = ASN1_STRING_length(gen_name->d.uniformResourceIdentifier);
                        copy_name = 1;
                        break;

                    case GEN_IPADD:
                        buf = malloc(16);   /* ###.###.###.###\0 is 16 characters */
                        inet_ntop(AF_INET, gen_name->d.iPAddress->data, buf, 16);
                        break;

                    default:
                        /* unsupported type -- ignore this entry */
                        continue;
                }

                if (copy_name)
                {
                    buf = malloc(name_len);
                    if (buf == NULL)
                    {
                        PyErr_SetString(PyExc_MemoryError, "Could not allocate memory for the copy of name.");
                        return NULL;
                    }
                    memcpy(buf, name, name_len);
                    buf[name_len] = 0;
                }

                /* build the tuple and put it in the list */
                /* we could use PyString_FromStringAndSize in the cases where we normally copy the name,
                 * but not in the others. */
                pybuf = PyString_FromString(buf);
                free(buf);
                if (pybuf == NULL)
                {
                    /* assuming that the only (or at least by far most likely) reason for this is
                     * running out of memory */
                    PyErr_SetString(PyExc_MemoryError, "Failed to create new string.");
                    return NULL;
                }
                tuple = PyTuple_Pack(2, PyInt_FromLong(gen_name->type), pybuf);
                if (tuple == NULL)
                {
                    /* assuming that the only (or at least by far most likely) reason for this is
                     * running out of memory */
                    PyErr_SetString(PyExc_MemoryError, "Failed to create new tuple.");
                    return NULL;
                }
                if (PyList_Append(result, tuple) == -1)
                {
                    /* the exception was already set, according to the Python docs */
                    return NULL;
                }
            }
            sk_GENERAL_NAME_free(alt_names);
            return result;
        }
    }

    /* no subjectAltName extension */
    Py_RETURN_NONE;
}


static char crypto_X509_verify_doc[] = "\n\
Verify the certificate against the CA certificates and the CRL.\n\
\n\
@param files: None or list of CA certificate and CRL files\n\
@param dirs: None or list of directories where CA certificates and CRLs will be sought\n\
@param check_crls: True if CRLs should be checked (then at least one must exist) or False if not.\n\
@return: 0 if the certificate is valid, otherwise an integer that is one of the X509_V_* macros in x509_vfy.h.\n\
";

static PyObject *
crypto_X509_verify(crypto_X509Obj *self, PyObject *args, PyObject *kwargs)
{
    X509_STORE *store = NULL;
    X509_STORE_CTX *context = NULL;
    X509_VERIFY_PARAM *verify_param = NULL;
    PyObject *result = NULL;

    int check_crls = 0;
    PyObject *file_list = NULL;
    PyObject *dir_list = NULL;

    static char *kwlist[] = {"check_crls", "files", "dirs"};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|iOO", kwlist,
				     &check_crls, &file_list, &dir_list))
    {
        return NULL;
    }

    if (file_list != NULL && !PySequence_Check(file_list)) {
        PyErr_SetString(PyExc_TypeError, "files should be a list or it shouldn't be specified");
	return NULL;
    }

    if (dir_list != NULL && !PySequence_Check(dir_list)) {
        PyErr_SetString(PyExc_TypeError, "dirs should be a list or it shouldn't be specified");
	return NULL;
    }

    store = X509_STORE_new();

    /* enable checking all CRLs */
    verify_param = X509_VERIFY_PARAM_new();
    if (check_crls)
        X509_VERIFY_PARAM_set_flags(verify_param, X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
    X509_STORE_set1_param(store, verify_param);

    /* load files */
    if (file_list != NULL) {
        int i;

        for (i = 0; i < PySequence_Size(file_list); i++)
        {
            PyObject *item;
            char *string;

            item = PySequence_GetItem(file_list, i);
            if (item == NULL)
                goto free_and_return;     /* Exception was set. Also, this shouldn't happen. */

            string = PyString_AsString(item);
            if (string == NULL)
                goto free_and_return;     /* Exception was set. */

            X509_STORE_load_locations(store, string, NULL);
        }
    }

    /* load directories */
    if (dir_list != NULL) {
        int i;

        for (i = 0; i < PySequence_Size(dir_list); i++)
        {
            PyObject *item;;
            char *string;

            item = PySequence_GetItem(dir_list, i);
            if (item == NULL)
                goto free_and_return;     /* Exception was set. Also, this shouldn't happen. */

            string = PyString_AsString(item);
            if (string == NULL)
                goto free_and_return;     /* Exception was set. */

            X509_STORE_load_locations(store, NULL, string);
        }
    }

    context = X509_STORE_CTX_new();
    X509_STORE_CTX_init(context, store, self->x509, NULL);
    X509_verify_cert(context);
    result = PyInt_FromLong(X509_STORE_CTX_get_error(context));

  free_and_return:
    X509_STORE_CTX_cleanup(context);
    X509_STORE_CTX_free(context);
    X509_VERIFY_PARAM_free(verify_param);

    return result;
}


static char crypto_X509_del_extension_doc[] = "\n\
Delete a specific extension of the certificate by index.\n\
\n\
:param index: The index of the extension to delete.\n\
:return: None\n\
";

static PyObject *
crypto_X509_del_extension(crypto_X509Obj *self, PyObject *args) {
    int loc;
    X509_EXTENSION *ext;

    if (!PyArg_ParseTuple(args, "i:del_extension", &loc)) {
        return NULL;
    }

    /* will return NULL if loc is out of the range of extensions */
    ext = X509_delete_ext(self->x509, loc);
    if (!ext) {
        PyErr_SetString(PyExc_IndexError, "extension index out of bounds");
	return NULL;
    }

    X509_EXTENSION_free(ext);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * ADD_METHOD(name) expands to a correct PyMethodDef declaration
 *   {  'name', (PyCFunction)crypto_X509_name, METH_VARARGS }
 * for convenience
 */
#define ADD_METHOD(name)        \
    { #name, (PyCFunction)crypto_X509_##name, METH_VARARGS, crypto_X509_##name##_doc }
#define ADD_METHOD_KW(name)     \
    { #name, (PyCFunction)crypto_X509_##name, METH_VARARGS | METH_KEYWORDS, crypto_X509_##name##_doc }
static PyMethodDef crypto_X509_methods[] =
{
    ADD_METHOD(get_version),
    ADD_METHOD(set_version),
    ADD_METHOD(get_serial_number),
    ADD_METHOD(set_serial_number),
    ADD_METHOD(get_issuer),
    ADD_METHOD(set_issuer),
    ADD_METHOD(get_subject),
    ADD_METHOD(set_subject),
    ADD_METHOD(get_pubkey),
    ADD_METHOD(set_pubkey),
    ADD_METHOD(get_notBefore),
    ADD_METHOD(set_notBefore),
    ADD_METHOD(get_notAfter),
    ADD_METHOD(set_notAfter),
    ADD_METHOD(gmtime_adj_notBefore),
    ADD_METHOD(gmtime_adj_notAfter),
    ADD_METHOD(get_signature_algorithm),
    ADD_METHOD(sign),
    ADD_METHOD(has_expired),
    ADD_METHOD(subject_name_hash),
    ADD_METHOD(digest),
    ADD_METHOD(add_extensions),
    ADD_METHOD(get_extension),
    ADD_METHOD(get_extension_count),
    ADD_METHOD(get_subject_alt_name),
    ADD_METHOD_KW(verify),
    ADD_METHOD(del_extension),
    { NULL, NULL }
};
#undef ADD_METHOD
#undef ADD_METHOD_KW


/*
 * Constructor for X509 objects, never called by Python code directly
 *
 * Arguments: cert    - A "real" X509 certificate object
 *            dealloc - Boolean value to specify whether the destructor should
 *                      free the "real" X509 object
 * Returns:   The newly created X509 object
 */
crypto_X509Obj *
crypto_X509_New(X509 *cert, int dealloc)
{
    crypto_X509Obj *self;

    self = PyObject_New(crypto_X509Obj, &crypto_X509_Type);

    if (self == NULL)
        return NULL;

    self->x509 = cert;
    self->dealloc = dealloc;

    return self;
}


static char crypto_X509_doc[] = "\n\
X509() -> X509 instance\n\
\n\
Create a new X509 object.\n\
\n\
@returns: The X509 object\n\
";

static PyObject *
crypto_X509_new(PyTypeObject *subtype, PyObject *args, PyObject *kwargs)
{
    if (!PyArg_ParseTuple(args, ":X509")) {
        return NULL;
    }

    return (PyObject *)crypto_X509_New(X509_new(), 1);
}


/*
 * Deallocate the memory used by the X509 object
 *
 * Arguments: self - The X509 object
 * Returns:   None
 */
static void
crypto_X509_dealloc(crypto_X509Obj *self)
{
    /* Sometimes we don't have to dealloc the "real" X509 pointer ourselves */
    if (self->dealloc)
        X509_free(self->x509);

    PyObject_Del(self);
}

PyTypeObject crypto_X509_Type = {
    PyOpenSSL_HEAD_INIT(&PyType_Type, 0)
    "X509",
    sizeof(crypto_X509Obj),
    0,
    (destructor)crypto_X509_dealloc,
    NULL, /* print */
    NULL, /* getattr */
    NULL, /* setattr */
    NULL, /* compare */
    NULL, /* repr */
    NULL, /* as_number */
    NULL, /* as_sequence */
    NULL, /* as_mapping */
    NULL, /* hash */
    NULL, /* call */
    NULL, /* str */
    NULL, /* getattro */
    NULL, /* setattro */
    NULL, /* as_buffer */
    Py_TPFLAGS_DEFAULT,
    crypto_X509_doc, /* doc */
    NULL, /* traverse */
    NULL, /* clear */
    NULL, /* tp_richcompare */
    0, /* tp_weaklistoffset */
    NULL, /* tp_iter */
    NULL, /* tp_iternext */
    crypto_X509_methods, /* tp_methods */
    NULL, /* tp_members */
    NULL, /* tp_getset */
    NULL, /* tp_base */
    NULL, /* tp_dict */
    NULL, /* tp_descr_get */
    NULL, /* tp_descr_set */
    0, /* tp_dictoffset */
    NULL, /* tp_init */
    NULL, /* tp_alloc */
    crypto_X509_new, /* tp_new */
};

/*
 * Initialize the X509 part of the crypto sub module
 *
 * Arguments: module - The crypto module
 * Returns:   None
 */
int
init_crypto_x509(PyObject *module)
{
    if (PyType_Ready(&crypto_X509_Type) < 0) {
        return 0;
    }

    if (PyModule_AddObject(module, "X509", (PyObject *)&crypto_X509_Type) != 0) {
        return 0;
    }

    if (PyModule_AddObject(module, "X509Type", (PyObject *)&crypto_X509_Type) != 0) {
        return 0;
    }

    return 1;
}

