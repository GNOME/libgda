/*
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include <jni-wrapper.h>
#include <gda-value.h>
#include <gda-server-provider.h>

gboolean jni_wrapper_describe_exceptions = FALSE;

static jclass            SQLException__class = NULL;
static JniWrapperMethod *get_message_method = NULL;
static JniWrapperMethod *get_error_code_method = NULL;
static JniWrapperMethod *get_sql_state_method = NULL;

#define JNI_WRAPPER_DEBUG
#undef JNI_WRAPPER_DEBUG

#ifdef G_OS_WIN32
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

/*
 * Returns: @iostring if it was not NULL, or a new string if a JAR has been added, or %NULL
 */
static GString *
locate_jars (GString *iostring, const gchar *path)
{
	if (g_str_has_suffix (path, ".jar") ||
	    g_str_has_suffix (path, ".JAR") ||
	    g_str_has_suffix (path, ".Jar")) {
		if (!iostring)
			iostring = g_string_new ("-Djava.class.path=");
		else
			g_string_append_c (iostring, PATH_SEPARATOR);
		g_string_append (iostring, path);
	}
	else {
		GDir *dir;
		dir = g_dir_open (path, 0, NULL);
		if (dir) {
			const gchar *file;
			for (file = g_dir_read_name (dir); file; file = g_dir_read_name (dir)) {
				if (g_str_has_suffix (file, ".jar") ||
				    g_str_has_suffix (file, ".JAR") ||
				    g_str_has_suffix (file, ".Jar")) {
					if (!iostring)
						iostring = g_string_new ("-Djava.class.path=");
					else
						g_string_append_c (iostring, PATH_SEPARATOR);
					g_string_append_printf (iostring, "%s%c%s", path,
								G_DIR_SEPARATOR, file);
				}
			}
			g_dir_close (dir);
		}
	}

	return iostring;
}

/**
 * jni_wrapper_create
 * @out_jvm: a pointer to store the JVM instance (NOT %NULL)
 * @lib_path: a path to where native libraries may be, or %NULL
 * @class_path: extra path to add to CLASSPATH, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new JAVA virtual machine and the new associated JNIEnv
 *
 * Returns: a new #JNIEnv, or %NULL if an error occurred
 */
JNIEnv *
jni_wrapper_create_vm (JavaVM **out_jvm, CreateJavaVMFunc create_func,
		       const gchar *lib_path, const gchar *class_path, GError **error)
{
	*out_jvm = NULL;
#ifndef JNI_VERSION_1_2
	g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISUSE_ERROR,
		     "%s", "Java 1.2 or more is needed");
	return NULL;
#else
	GString *classpath = NULL;
	gint nopt;
	JavaVMOption options[4];
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;
	long result;
	const gchar *tmp;

	if (!create_func) {
		g_set_error (error,GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", 
			     "The JNI_CreateJavaVM is not identified (as the create_func argument)");
		return NULL;
	}

	gchar * confdir;
	confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "libgda", NULL);
	if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
		g_free (confdir);
		confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".libgda", NULL);
	}
	classpath = locate_jars (classpath, confdir);
	g_free (confdir);

	if (class_path) {
		if (!classpath)
			classpath = g_string_new ("-Djava.class.path=");
		g_string_append_c (classpath, PATH_SEPARATOR);
		g_string_append (classpath, class_path);
	}

	if ((tmp = g_getenv ("CLASSPATH")) && *tmp) {
		gchar **arr = g_strsplit (tmp, ":", 0);
		gchar **ptr;
		for (ptr = arr; ptr && *ptr; ptr++)
			classpath = locate_jars (classpath, *ptr);
		g_strfreev (arr);
	}

	nopt = 0;
	if (classpath)
		options[nopt++].optionString = classpath->str;
	options[nopt++].optionString = "-Djava.compiler=NONE";

	if (lib_path) 
		options[nopt++].optionString = g_strdup_printf ("-Djava.library.path=%s", lib_path);
	vm_args.nOptions = nopt;

	if (g_getenv ("GDA_JAVA_OPTION")) {
		const gchar *opt = g_getenv ("GDA_JAVA_OPTION");
		options[vm_args.nOptions].optionString = (gchar*) opt;
		vm_args.nOptions++;
	}
	vm_args.version = JNI_VERSION_1_2;
	vm_args.options = options;

#ifdef JNI_WRAPPER_DEBUG
	{
		gint i;
		for (i = 0; i < vm_args.nOptions; i++)
			g_print ("VMOption %d: %s\n", i, options[i].optionString);
	}
#endif

	vm_args.ignoreUnrecognized = JNI_FALSE;
	result = create_func (&jvm,(void **) &env, &vm_args);
	g_string_free (classpath, TRUE);
	g_free (options[2].optionString);
	if ((result == JNI_ERR) || !env) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", "Can't invoke the JVM");
		return NULL;
	}

	*out_jvm = jvm;

#ifdef JNI_WRAPPER_DEBUG
	g_print ("JVM loaded\n");
#endif

	jclass klass;
	klass = jni_wrapper_class_get (env, "java/lang/Throwable", NULL);
	if (!klass) 
		g_warning ("Error loading '%s' class (error messages won't be detailed)",
			   "java.lang.Throwable");
	else {
		get_message_method = jni_wrapper_method_create (env, klass, 
								"getMessage", "()Ljava/lang/String;", 
								FALSE, NULL);
		if (!get_message_method)
			g_warning ("Error loading '%s' method (error messages won't be detailed)",
				   "java.lang.Throwable.getMessage");
		(*env)->DeleteGlobalRef (env, klass);
	}

	klass = jni_wrapper_class_get (env, "java/sql/SQLException", NULL);
	if (!klass) 
		g_warning ("Error loading '%s' class (error messages won't be detailed)",
			   "java.sql.SqlException");
	else {
		SQLException__class = klass;
		get_error_code_method = jni_wrapper_method_create (env, SQLException__class, 
								   "getErrorCode", "()I", 
								   FALSE, NULL);
		if (!get_error_code_method)
			g_warning ("Error loading '%s' method (error messages won't be detailed)",
				   "java.SQLException.getErrorCode");

		get_sql_state_method = jni_wrapper_method_create (env, SQLException__class, 
								  "getSQLState", "()Ljava/lang/String;", 
								  FALSE, NULL);

		if (!get_sql_state_method)
			g_warning ("Error loading '%s' method (error messages won't be detailed)",
				   "java.SQLException.getSQLState");
	}

	return env;
#endif
}

/**
 * jni_wrapper_destroy
 */
void
jni_wrapper_destroy_vm (JavaVM *jvm)
{
	g_return_if_fail (jvm);
	(*jvm)->DestroyJavaVM (jvm);
#ifdef JNI_WRAPPER_DEBUG
	g_print ("JVM destroyed\n");
#endif
}

/**
 * jni_wrapper_class_create
 *
 * Returns: a jclass object as a JNI global reference
 */
jclass
jni_wrapper_class_get (JNIEnv *jenv, const gchar *class_name, GError **error)
{
	jclass cls, gcls;

	g_return_val_if_fail (jenv, NULL);
	cls = (*jenv)->FindClass (jenv, class_name);
	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) {
#ifdef JNI_WRAPPER_DEBUG
		g_warning ("Class '%s' NOT found\n", class_name);
#endif
		return NULL;
	}

#ifdef JNI_WRAPPER_DEBUG
	g_print ("Class '%s' found\n", class_name);
#endif
	gcls = (*jenv)->NewGlobalRef (jenv, cls);
	(*jenv)-> DeleteLocalRef (jenv, cls);

	return gcls;
}

/**
 * jni_wrapper_instantiate_object
 *
 * Returns: a new GValue with a pointer to the new java object
 */
GValue *
jni_wrapper_instantiate_object (JNIEnv *jenv, jclass klass, const gchar *signature, 
				GError **error, ...)
{
	JniWrapperMethod *method;
	GValue *retval;
	va_list args;
	JavaVM *jvm;

	g_return_val_if_fail (klass, NULL);

	method = jni_wrapper_method_create (jenv, klass, "<init>", signature, FALSE, error);
	if (!method)
		return NULL;

	if ((*jenv)->GetJavaVM (jenv, &jvm))
		g_error ("Could not attach JAVA virtual machine's current thread");
	
	retval = g_new0 (GValue, 1);
	g_value_init (retval, GDA_TYPE_JNI_OBJECT);

	va_start (args, error);
	gda_value_set_jni_object (retval, jvm, jenv, 
				  (*jenv)->NewObjectV (jenv, klass, method->mid, args));
	va_end (args);
	
	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) {
		gda_value_free (retval);
		retval = NULL;
	}

	jni_wrapper_method_free (jenv, method);
	return retval;
}

/**
 * jni_wrapper_handle_exception
 * @jenv:
 * @out_error_code: place to store an error code, or %NULL
 * @out_sql_state: place to store a (new) sql state string, or %NULL
 * @error: place to store errors, or %NULL
 *
 * Returns: TRUE if there was an exception, and clears the exceptions
 */
gboolean
jni_wrapper_handle_exception (JNIEnv *jenv, gint *out_error_code, gchar **out_sql_state, GError **error)
{
	jthrowable exc;
	GValue *exc_value = NULL;

	if (out_error_code)
		*out_error_code = 0;
	if (out_sql_state)
		*out_sql_state = NULL;

	exc = (*jenv)->ExceptionOccurred (jenv);
	if (!exc)
		return FALSE; /* no exception */

	if (jni_wrapper_describe_exceptions) {
		static gint c = 0;
		g_print ("JAVA EXCEPTION %d\n", c);
		(*jenv)->ExceptionDescribe (jenv);
		g_print ("JAVA EXCEPTION %d\n", c);
		c++;
	}

	if (out_error_code || out_sql_state || error) {
		JavaVM *jvm;
		exc_value = g_new0 (GValue, 1);
		g_value_init (exc_value, GDA_TYPE_JNI_OBJECT);
		if ((*jenv)->GetJavaVM (jenv, &jvm))
			g_error ("Could not attach JAVA virtual machine's current thread");

		gda_value_set_jni_object (exc_value, jvm, jenv, exc);
	}
	(*jenv)->ExceptionClear (jenv);

	if (out_error_code || out_sql_state) {
		if ((*jenv)->IsInstanceOf (jenv, exc, SQLException__class)) {
			if (out_error_code) {
				GValue *res;				
				
				res = jni_wrapper_method_call (jenv, get_error_code_method, exc_value, 
							       NULL, NULL, NULL);
				if (res) {
					if (G_VALUE_TYPE (res) == G_TYPE_INT)
						*out_error_code = g_value_get_int (res);
					gda_value_free (res);
				}
			}
			if (out_sql_state) {
				GValue *res;				
				
				res = jni_wrapper_method_call (jenv, get_sql_state_method, exc_value, 
							       NULL, NULL, NULL);
				if (res) {
					if (G_VALUE_TYPE (res) == G_TYPE_STRING)
						*out_sql_state = g_value_dup_string (res);
					gda_value_free (res);
				}
			}
		}
	}
	(*jenv)->DeleteLocalRef(jenv, exc);

	if (error) {
		if (get_message_method) {
			GValue *res;

			res = jni_wrapper_method_call (jenv, get_message_method, exc_value, NULL, NULL, NULL);

			if (res) {
				if (G_VALUE_TYPE (res) == G_TYPE_STRING) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     "%s", g_value_get_string (res));
					gda_value_free (res);
				}
				else {
					gda_value_free (res);
					goto fallback;
				}
			}
			else
				goto fallback;
		}
		else
			goto fallback;
	}

	gda_value_free (exc_value);

	return TRUE;

 fallback:
	g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
		     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
		     "%s", "An exception occurred");
	gda_value_free (exc_value);
	(*jenv)->DeleteLocalRef(jenv, exc);
	return TRUE;
}

/**
 * jni_wrapper_method_create
 * @jenv:
 * @klass:
 * @method_name: name of the requested method
 * @signature: methods's signature (use "java -p -s <classname>" to get it)
 * @is_static: TRUE if the requested method is a class method (and FALSE if it's an instance method)
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #JniWrapperMethod structure
 */
JniWrapperMethod *
jni_wrapper_method_create (JNIEnv *jenv, jclass klass, 
			   const gchar *method_name, const gchar *signature,
			   gboolean is_static, GError **error)
{
	JniWrapperMethod *method;
	jmethodID mid;
	const gchar *ptr;
	g_return_val_if_fail (klass, NULL);
	if (is_static) 
		mid = (*jenv)->GetStaticMethodID (jenv, klass, method_name, signature);
	else 
		mid = (*jenv)->GetMethodID (jenv, klass, method_name, signature);
	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error))
		return NULL;

	method = g_new0 (JniWrapperMethod, 1);
	method->klass = (*jenv)->NewGlobalRef (jenv, klass);
	method->is_static = is_static;
	method->mid = mid;

	for (ptr = signature; *ptr && (*ptr != ')'); ptr++);
	g_assert (*ptr);
	ptr++;
	method->ret_type = g_strdup (ptr);

	return method;
}

/**
 * jni_wrapper_method_call
 * @jenv:
 * @method:
 * @object: instance on which the method is executed, or %NULL for a static method call
 * @out_error_code: place to store an error code, or %NULL
 * @out_sql_state: place to store a (new) sql state string, or %NULL
 * @error: a place to store errors, or %NULL
 * @...: methods' arguments, USED AS IS by the JNI
 *
 * Executes an instance method.
 *
 * Returns: a GValue (may return a GDA_TYPE_NULL value for functions returning void), or %NULL if an error occurred
 */
GValue *
jni_wrapper_method_call (JNIEnv *jenv, JniWrapperMethod *method, GValue *object, 
			 gint *out_error_code, gchar **out_sql_state, GError **error, ...)
{
	GValue *retval;
	va_list args;
	jobject jobj = NULL;
	g_return_val_if_fail (method, NULL);
	if (method->is_static)
		g_return_val_if_fail (!object, NULL);
	else {
		g_return_val_if_fail (object, NULL);
		g_return_val_if_fail (G_VALUE_TYPE (object) == GDA_TYPE_JNI_OBJECT, NULL);
		jobj = gda_value_get_jni_object (object);
		g_return_val_if_fail (jobj, NULL);
	}

	if (out_error_code)
		*out_error_code = 0;
	if (out_sql_state)
		*out_sql_state = NULL;

	retval = g_new0 (GValue, 1);

	/* actual method call */
	va_start (args, error);
	switch (*method->ret_type) {
	case 'V':
		if (method->is_static)
			(*jenv)->CallStaticVoidMethodV (jenv, method->klass, method->mid, args);
		else
			(*jenv)->CallVoidMethodV (jenv, jobj, method->mid, args);
		gda_value_set_null (retval);
		break;
	case '[':
	case 'L': 
		if (!strcmp (method->ret_type, "Ljava/lang/String;")) {
			jstring string;
			gchar *str;
			if (method->is_static)
				string = (*jenv)->CallStaticObjectMethodV (jenv, method->klass, 
									   method->mid, args);
			else
				string = (*jenv)->CallObjectMethodV (jenv, jobj, method->mid, args);
			if (string) {
				gint len, ulen;
				g_value_init (retval, G_TYPE_STRING);
				len = (*jenv)->GetStringUTFLength (jenv, string);
				if ((*jenv)->ExceptionCheck (jenv))
					break;
				ulen = (*jenv)->GetStringLength (jenv, string);
				if ((*jenv)->ExceptionCheck (jenv))
					break;
				str = g_new (gchar, len + 1);
				str [len] = 0;
				if (ulen > 0)
					(*jenv)->GetStringUTFRegion (jenv, string, 0, ulen, str);
				if ((*jenv)->ExceptionCheck (jenv)) {
					g_free (str);
					break;
				}
				g_value_take_string (retval, str);

				(*jenv)-> DeleteLocalRef (jenv, string);
			}
			else
				gda_value_set_null (retval);
		}
		else {
			JavaVM *jvm;
			if ((*jenv)->GetJavaVM (jenv, &jvm))
				g_error ("Could not attach JAVA virtual machine's current thread");
			g_value_init (retval, GDA_TYPE_JNI_OBJECT);
			if (method->is_static)
				gda_value_set_jni_object (retval, jvm, jenv, 
							  (*jenv)->CallStaticObjectMethodV (jenv, method->klass, 
											    method->mid, args));
			else
				gda_value_set_jni_object (retval, jvm, jenv, 
							  (*jenv)->CallObjectMethodV (jenv, 
										      jobj, method->mid, args));
		}
		break;
	case 'Z':
		g_value_init (retval, G_TYPE_BOOLEAN);
		if (method->is_static)
			g_value_set_boolean (retval,
					     (*jenv)->CallStaticBooleanMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_boolean (retval,
					     (*jenv)->CallBooleanMethodV (jenv, jobj, method->mid, args));
		break;
	case 'B':
		g_value_init (retval, G_TYPE_CHAR);
		if (method->is_static)
			g_value_set_schar (retval,
					   (*jenv)->CallStaticByteMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_schar (retval,
					   (*jenv)->CallByteMethodV (jenv, jobj, method->mid, args));
		break;
	case 'C':
		g_value_init (retval, G_TYPE_INT); // FIXME: should be an unsigned 16 bits value
		if (method->is_static)
			g_value_set_int (retval,
					 (*jenv)->CallStaticCharMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_int (retval,
					 (*jenv)->CallCharMethodV (jenv, jobj, method->mid, args));
		break;
	case 'S':
		g_value_init (retval, G_TYPE_INT); // FIXME: should be a signed 16 bits value
		if (method->is_static)
			g_value_set_int (retval,
					 (*jenv)->CallStaticShortMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_int (retval,
					 (*jenv)->CallShortMethodV (jenv, jobj, method->mid, args));
		break;
	case 'I':
		g_value_init (retval, G_TYPE_INT);
		if (method->is_static)
			g_value_set_int (retval, 
					 (*jenv)->CallStaticIntMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_int (retval, 
					 (*jenv)->CallIntMethodV (jenv, jobj, method->mid, args));
		break;
	case 'J':
		g_value_init (retval, G_TYPE_INT64);
		if (method->is_static)
			g_value_set_int64 (retval,
				   (*jenv)->CallStaticLongMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_int64 (retval,
					   (*jenv)->CallLongMethodV (jenv, jobj, method->mid, args));
		break;
	case 'F':
		g_value_init (retval, G_TYPE_FLOAT);
		if (method->is_static)
			g_value_set_float (retval,
					   (*jenv)->CallStaticFloatMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_float (retval,
					   (*jenv)->CallFloatMethodV (jenv, jobj, method->mid, args));
		break;
	case 'D':
		g_value_init (retval, G_TYPE_DOUBLE);
		if (method->is_static)
			g_value_set_double (retval,
					    (*jenv)->CallStaticDoubleMethodV (jenv, method->klass, method->mid, args));
		else
			g_value_set_double (retval,
					    (*jenv)->CallDoubleMethodV (jenv, jobj, method->mid, args));
		break;
	default:
		(*jenv)->FatalError (jenv, "illegal descriptor");
	}
	va_end (args);

	if (jni_wrapper_handle_exception (jenv, out_error_code, out_sql_state, error)) {
		gda_value_free (retval);
		return NULL;
	}
 
	return retval;
}

/**
 * jni_wrapper_method_free
 * @jenv:
 * @method:
 *
 */
void
jni_wrapper_method_free (JNIEnv *jenv, JniWrapperMethod *method)
{
	g_return_if_fail (method);
	(*jenv)->DeleteGlobalRef (jenv, method->klass);
	g_free (method->ret_type);
	g_free (method);
}

/**
 * jni_wrapper_field_create
 * @jenv:
 * @klass:
 * @field_name: name of the requested field
 * @signature: fields's signature (use "java -p -s <classname>" to get it)
 * @is_static: TRUE if the requested field is a class field (and FALSE if it's an instance field)
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #JniWrapperField structure
 */
JniWrapperField *
jni_wrapper_field_create (JNIEnv *jenv, jclass klass, 
			   const gchar *field_name, const gchar *signature,
			   gboolean is_static, GError **error)
{
	JniWrapperField *field;
	jfieldID fid;

	g_return_val_if_fail (klass, NULL);
	if (is_static) 
		fid = (*jenv)->GetStaticFieldID (jenv, klass, field_name, signature);
	else 
		fid = (*jenv)->GetFieldID (jenv, klass, field_name, signature);
	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error))
		return NULL;

	field = g_new0 (JniWrapperField, 1);
	field->klass = (*jenv)->NewGlobalRef (jenv, klass);
	field->is_static = is_static;
	field->fid = fid;

	field->type = g_strdup (signature);

	return field;
}


/**
 * jni_wrapper_field_get
 * @jenv:
 * @field:
 * @object: instance of which the field is to be obtained, or %NULL for a static field
 * @error: a place to store errors, or %NULL
 *
 * Set the value of a static field or an instance's field.
 *
 * Returns: a GValue (may return a GDA_TYPE_NULL value for functions returning void), or %NULL if an error occurred
 */
GValue *
jni_wrapper_field_get (JNIEnv *jenv, JniWrapperField *field, GValue *object, GError **error)
{
	GValue *retval;
	jobject jobj = NULL;
	g_return_val_if_fail (field, NULL);
	if (field->is_static)
		g_return_val_if_fail (!object, NULL);
	else {
		g_return_val_if_fail (object, NULL);
		g_return_val_if_fail (G_VALUE_TYPE (object) == GDA_TYPE_JNI_OBJECT, NULL);
		jobj = gda_value_get_jni_object (object);
		g_return_val_if_fail (jobj, NULL);
	}

	retval = g_new0 (GValue, 1);
	
	switch (*field->type) {
	case '[':
	case 'L': 
		if (!strcmp (field->type, "Ljava/lang/String;")) {
			jstring string;
			gchar *str;
			if (field->is_static)
				string = (*jenv)->GetStaticObjectField (jenv, field->klass, field->fid);
			else
				string = (*jenv)->GetObjectField (jenv, jobj, field->fid);
			if (string) {
				gint len, ulen;
				g_value_init (retval, G_TYPE_STRING);
				len = (*jenv)->GetStringUTFLength (jenv, string);
				if ((*jenv)->ExceptionCheck (jenv))
					break;
				ulen = (*jenv)->GetStringLength (jenv, string);
				if ((*jenv)->ExceptionCheck (jenv))
					break;
				
				str = g_new (gchar, len + 1);
				str [len] = 0;
				if (ulen > 0)
					(*jenv)->GetStringUTFRegion (jenv, string, 0, ulen, str);
				if ((*jenv)->ExceptionCheck (jenv)) {
					g_free (str);
					break;
				}
				g_value_take_string (retval, str);

				(*jenv)-> DeleteLocalRef (jenv, string);
			}
			else
				gda_value_set_null (retval);
		}
		else {
			JavaVM *jvm;
			if ((*jenv)->GetJavaVM (jenv, &jvm))
				g_error ("Could not attach JAVA virtual machine's current thread");
			g_value_init (retval, GDA_TYPE_JNI_OBJECT);
			if (field->is_static)
				gda_value_set_jni_object (retval, jvm, jenv, 
							  (*jenv)->GetStaticObjectField (jenv, field->klass, field->fid));
			else
				gda_value_set_jni_object (retval, jvm, jenv, 
							  (*jenv)->GetObjectField (jenv, jobj, field->fid));
		}
		break;
	case 'Z':
		g_value_init (retval, G_TYPE_BOOLEAN);
		if (field->is_static)
			g_value_set_boolean (retval,
					     (*jenv)->GetStaticBooleanField (jenv, field->klass, field->fid));
		else
			g_value_set_boolean (retval,
					     (*jenv)->GetBooleanField (jenv, jobj, field->fid));
		break;
	case 'B':
		g_value_init (retval, G_TYPE_CHAR);
		if (field->is_static)
			g_value_set_schar (retval,
					   (*jenv)->GetStaticByteField (jenv, field->klass, field->fid));
		else
			g_value_set_schar (retval,
					   (*jenv)->GetByteField (jenv, jobj, field->fid));
		break;
	case 'C':
		g_value_init (retval, G_TYPE_INT); // FIXME: should be an unsigned 16 bits value
		if (field->is_static)
			g_value_set_int (retval,
					 (*jenv)->GetStaticCharField (jenv, field->klass, field->fid));
		else
			g_value_set_int (retval,
					 (*jenv)->GetCharField (jenv, jobj, field->fid));
		break;
	case 'S':
		g_value_init (retval, G_TYPE_INT); // FIXME: should be a signed 16 bits value
		if (field->is_static)
			g_value_set_int (retval,
					 (*jenv)->GetStaticShortField (jenv, field->klass, field->fid));
		else
			g_value_set_int (retval,
					 (*jenv)->GetShortField (jenv, jobj, field->fid));
		break;
	case 'I':
		g_value_init (retval, G_TYPE_INT);
		if (field->is_static)
			g_value_set_int (retval, 
					 (*jenv)->GetStaticIntField (jenv, field->klass, field->fid));
		else
			g_value_set_int (retval, 
					 (*jenv)->GetIntField (jenv, jobj, field->fid));
		break;
	case 'J':
		g_value_init (retval, G_TYPE_INT64);
		if (field->is_static)
			g_value_set_int64 (retval,
				   (*jenv)->GetStaticLongField (jenv, field->klass, field->fid));
		else
			g_value_set_int64 (retval,
					   (*jenv)->GetLongField (jenv, jobj, field->fid));
		break;
	case 'F':
		g_value_init (retval, G_TYPE_FLOAT);
		if (field->is_static)
			g_value_set_float (retval,
					   (*jenv)->GetStaticFloatField (jenv, field->klass, field->fid));
		else
			g_value_set_float (retval,
					   (*jenv)->GetFloatField (jenv, jobj, field->fid));
		break;
	case 'D':
		g_value_init (retval, G_TYPE_DOUBLE);
		if (field->is_static)
			g_value_set_double (retval,
					    (*jenv)->GetStaticDoubleField (jenv, field->klass, field->fid));
		else
			g_value_set_double (retval,
					    (*jenv)->GetDoubleField (jenv, jobj, field->fid));
		break;
	default:
		(*jenv)->FatalError (jenv, "illegal descriptor");
	}

	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) {
		gda_value_free (retval);
		return NULL;
	}
 
	return retval;
}

/**
 * jni_wrapper_field_set
 * @jenv:
 * @field:
 * @object: instance of which the field is to be obtained, or %NULL for a static field
 * @value: a #GValue to set the field to
 * @error: a place to store errors, or %NULL
 *
 * Set the value of a static field or an instance's field.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
jni_wrapper_field_set (JNIEnv *jenv, JniWrapperField *field, 
		       GValue *object, const GValue *value, GError **error)
{
	jobject jobj = NULL;
		
	g_return_val_if_fail (field, FALSE);
	g_return_val_if_fail (value, FALSE);
	if (field->is_static)
		g_return_val_if_fail (!object, FALSE);
	else {
		g_return_val_if_fail (object, FALSE);
		g_return_val_if_fail (G_VALUE_TYPE (object) == GDA_TYPE_JNI_OBJECT, FALSE);
		jobj = gda_value_get_jni_object (object);
		g_return_val_if_fail (jobj, FALSE);
	}

	switch (*field->type) {
	case '[':
	case 'L': 
		if (!strcmp (field->type, "Ljava/lang/String;")) {
			jstring string;

			if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
				string = (*jenv)->NewStringUTF (jenv, g_value_get_string (value));
				if (field->is_static)
					(*jenv)->SetStaticObjectField (jenv, field->klass, field->fid, string);
				else
					(*jenv)->SetObjectField (jenv, jobj, field->fid, string);
				(*jenv)-> DeleteLocalRef (jenv, string);
			}
			else if (G_VALUE_TYPE (value) == GDA_TYPE_NULL) {
				if (field->is_static)
					(*jenv)->SetStaticObjectField (jenv, field->klass, field->fid, NULL);
				else
					(*jenv)->SetObjectField (jenv, jobj, field->fid, NULL);
			}
			else
				goto wrong_type;
		}
		else {
			if (G_VALUE_TYPE (value) == GDA_TYPE_JNI_OBJECT) {
				if (field->is_static)
					(*jenv)->SetStaticObjectField (jenv, field->klass, field->fid,
								       gda_value_get_jni_object (value));
				else
					(*jenv)->SetObjectField (jenv, jobj, field->fid,
								 gda_value_get_jni_object (value));
			}
			else if (G_VALUE_TYPE (value) == 0) {
				if (field->is_static)
					(*jenv)->SetStaticObjectField (jenv, field->klass, field->fid, NULL);
				else
					(*jenv)->SetObjectField (jenv, jobj, field->fid, NULL);
			}
			else
				goto wrong_type;
		}
		break;
	case 'Z':
		if (G_VALUE_TYPE (value) != G_TYPE_BOOLEAN)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticBooleanField (jenv, field->klass, field->fid, g_value_get_boolean (value));
		else
			(*jenv)->SetBooleanField (jenv, jobj, field->fid, g_value_get_boolean (value));
		break;
	case 'B':
		if (G_VALUE_TYPE (value) != G_TYPE_CHAR)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticByteField (jenv, field->klass, field->fid, g_value_get_schar (value));
		else
			(*jenv)->SetByteField (jenv, jobj, field->fid, g_value_get_schar (value));
		break;
	case 'C':
		if (G_VALUE_TYPE (value) != G_TYPE_INT)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticCharField (jenv, field->klass, field->fid, g_value_get_int (value));
		else
			(*jenv)->SetCharField (jenv, jobj, field->fid, g_value_get_int (value));
		break;
	case 'S':
		if (G_VALUE_TYPE (value) != G_TYPE_INT)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticShortField (jenv, field->klass, field->fid, g_value_get_int (value));
		else
			(*jenv)->SetShortField (jenv, jobj, field->fid, g_value_get_int (value));
		break;
	case 'I':
		if (G_VALUE_TYPE (value) != G_TYPE_INT)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticIntField (jenv, field->klass, field->fid, g_value_get_int (value));
		else
			(*jenv)->SetIntField (jenv, jobj, field->fid, g_value_get_int (value));
		break;
	case 'J':
		if (G_VALUE_TYPE (value) != G_TYPE_INT64)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticLongField (jenv, field->klass, field->fid, g_value_get_int64 (value));
		else
			(*jenv)->SetLongField (jenv, jobj, field->fid, g_value_get_int64 (value));
		break;
	case 'F':
		if (G_VALUE_TYPE (value) != G_TYPE_FLOAT)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticFloatField (jenv, field->klass, field->fid, g_value_get_float (value));
		else
			(*jenv)->SetFloatField (jenv, jobj, field->fid, g_value_get_float (value));
		break;
	case 'D':
		if (G_VALUE_TYPE (value) != G_TYPE_DOUBLE)
			goto wrong_type;
		if (field->is_static)
			(*jenv)->SetStaticDoubleField (jenv, field->klass, field->fid, g_value_get_double (value));
		else
			(*jenv)->SetDoubleField (jenv, jobj, field->fid, g_value_get_double (value));
		break;
	default:
		(*jenv)->FatalError (jenv, "illegal descriptor");
	}

	if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) 
		return FALSE;
 
	return TRUE;

 wrong_type:
	g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
		     "%s", "Wrong value type");
	return FALSE;
}

/**
 * jni_wrapper_field_free
 * @jenv:
 * @field:
 *
 */
void
jni_wrapper_field_free (JNIEnv *jenv, JniWrapperField *field)
{
	g_return_if_fail (field);
	(*jenv)->DeleteGlobalRef (jenv, field->klass);
	g_free (field->type);
	g_free (field);
}

/*
 * jobject wrapper in a GValue
 */
jobject
gda_value_get_jni_object (const GValue *value)
{
	GdaJniObject *jnio = (GdaJniObject*) g_value_get_boxed (value);

	g_return_val_if_fail (jnio, NULL);
	return jnio->jobj;
}

GValue  *
gda_value_new_jni_object (JavaVM *jvm, JNIEnv *env, jobject jni_object)
{
	GValue *value;
	g_return_val_if_fail (jvm, NULL);
	g_return_val_if_fail (env, NULL);
	value = g_new0 (GValue, 1);
	g_value_init (value, GDA_TYPE_JNI_OBJECT);
	gda_value_set_jni_object (value, jvm, env, jni_object);
	return value;
}

void
gda_value_set_jni_object (GValue *value, JavaVM *jvm, JNIEnv *env, jobject jni_object)
{
	GdaJniObject *jnio;

	jnio = g_new (GdaJniObject, 1);
	jnio->jvm = jvm;
	if (jni_object) 
		jnio->jobj = (*env)->NewGlobalRef (env, jni_object);
	else
		jnio->jobj = NULL;
	g_value_set_boxed (value, jnio);
}

GType
gda_jni_object_get_type (void)
{
	static GType type = 0;
	
        if (G_UNLIKELY (type == 0)) {
                type = g_boxed_type_register_static ("GdaJniObject",
                                                     (GBoxedCopyFunc) gda_jni_object_copy,
                                                     (GBoxedFreeFunc) gda_jni_object_free);
        }

        return type;
}

gpointer
gda_jni_object_copy (gpointer boxed)
{
	JNIEnv *env;
	GdaJniObject *src = (GdaJniObject*) boxed;
	GdaJniObject *copy;
	jint atres;

	atres = (*src->jvm)->GetEnv (src->jvm, (void**) &env, JNI_VERSION_1_2);
	if (atres == JNI_EDETACHED) {
		if ((*src->jvm)->AttachCurrentThread (src->jvm, (void**) &env, NULL) < 0)
			g_error ("Could not attach JAVA virtual machine's current thread");
	}
	else if (atres == JNI_EVERSION)
		g_error ("Could not attach JAVA virtual machine's current thread");

	copy = g_new (GdaJniObject, 1);
	copy->jvm = src->jvm;
	copy->jobj = (*env)->NewGlobalRef(env, src->jobj);

	if (atres == JNI_EDETACHED)
		(*src->jvm)->DetachCurrentThread (src->jvm);

	return (gpointer) copy;
}

void
gda_jni_object_free (gpointer boxed)
{
	JNIEnv *env;
	GdaJniObject *jnio = (GdaJniObject*) boxed;

	if (jnio->jobj) {
		jint atres;
		
		atres = (*jnio->jvm)->GetEnv (jnio->jvm, (void**) &env, JNI_VERSION_1_2);
		if (atres == JNI_EDETACHED) {
			if ((*jnio->jvm)->AttachCurrentThread (jnio->jvm, (void**) &env, NULL) < 0)
				g_error ("Could not attach JAVA virtual machine's current thread");
		}
		else if (atres == JNI_EVERSION)
			g_error ("Could not attach JAVA virtual machine's current thread");
		
		(*env)->DeleteGlobalRef (env, jnio->jobj);
		if (atres == JNI_EDETACHED)
			(*jnio->jvm)->DetachCurrentThread (jnio->jvm);
	}
	g_free (jnio);
}

jlong
jni_cpointer_to_jlong (gconstpointer c_pointer)
{
	jlong retval;

#if SIZEOF_INT_P == 4
	gint32 i32;
	i32 = (gint32) c_pointer;
	retval = i32;
#else
	retval = (jlong) c_pointer;
#endif
	return retval;
}

gconstpointer
jni_jlong_to_cpointer (jlong value)
{
	gconstpointer retval;
#if SIZEOF_INT_P == 4
	gint32 i32;
	i32 = (gint32) value;
	retval = (gconstpointer) i32;
#else
	retval = (gconstpointer) value;
#endif
	return retval;
}
