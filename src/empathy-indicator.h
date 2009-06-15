#ifndef EMPATHY_INDICATOR_H
#define EMPATHY_INDICATOR_H

#include <glib.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_INDICATOR         (empathy_indicator_get_type ())
#define EMPATHY_INDICATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_INDICATOR, EmpathyIndicator))
#define EMPATHY_INDICATOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_INDICATOR, EmpathyIndicatorClass))
#define EMPATHY_IS_INDICATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_INDICATOR))
#define EMPATHY_IS_INDICATOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_INDICATOR))
#define EMPATHY_INDICATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_INDICATOR, EmpathyIndicatorClass))

typedef struct _EmpathyIndicator      EmpathyIndicator;
typedef struct _EmpathyIndicatorClass EmpathyIndicatorClass;

struct _EmpathyIndicator {
	GObject parent;
	gpointer priv;
};

struct _EmpathyIndicatorClass {
	GObjectClass parent_class;
};

GType              empathy_indicator_get_type (void) G_GNUC_CONST;
EmpathyIndicator  *empathy_indicator_new (EmpathyContact *sender,
                        const gchar *body, const gchar *type);
void               empathy_indicator_show (EmpathyIndicator *e_indicator);
void               empathy_indicator_hide (EmpathyIndicator *e_indicator);
void               empathy_indicator_update (EmpathyIndicator *e_indicator,
                        const gchar *body);
EmpathyContact    *empathy_indicator_get_contact (EmpathyIndicator *e_indicator);

G_END_DECLS


#endif /* EMPATHY-INDICATOR_H */
