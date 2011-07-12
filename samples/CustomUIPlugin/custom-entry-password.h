#ifndef __CUSTOM_ENTRY_PASSWORD_H_
#define __CUSTOM_ENTRY_PASSWORD_H_

#include <libgda-ui/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define CUSTOM_ENTRY_PASSWORD_TYPE          (custom_entry_password_get_type())
#define CUSTOM_ENTRY_PASSWORD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, custom_entry_password_get_type(), CustomEntryPassword)
#define CUSTOM_ENTRY_PASSWORD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, custom_entry_password_get_type (), CustomEntryPasswordClass)
#define IS_CUSTOM_ENTRY_PASSWORD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, custom_entry_password_get_type ())


typedef struct _CustomEntryPassword CustomEntryPassword;
typedef struct _CustomEntryPasswordClass CustomEntryPasswordClass;
typedef struct _CustomEntryPasswordPrivate CustomEntryPasswordPrivate;


/* struct for the object's data */
struct _CustomEntryPassword
{
	GdauiEntryWrapper              object;
	CustomEntryPasswordPrivate    *priv;
};

/* struct for the object's class */
struct _CustomEntryPasswordClass
{
	GdauiEntryWrapperClass         parent_class;
};

GType        custom_entry_password_get_type (void) G_GNUC_CONST;
GtkWidget   *custom_entry_password_new      (GdaDataHandler *dh, GType type, const gchar *options);


G_END_DECLS

#endif
