#include <loginpanel.h>
#include <mainwindow.h>
#include <qq.h>

/*
 * The global value
 */
extern QQInfo *info;

static void qq_loginpanelclass_init(QQLoginPanelClass *c);
static void qq_loginpanel_init(QQLoginPanel *obj);
static void qq_loginpanel_destroy(GtkObject *obj);
static void login_cb(CallBackResult cbr, gpointer redata, gpointer usrdata);

/*
 * May be I should put thest two variables to the QQLoingPanleClass.
 * But, put them here can be very convenience.
 */
static const gchar *status[] = {"online", "hidden", "away", "offline", NULL};
static const gchar *status_label[] = {" 在线", " 隐身", " 离开", " 离线"
					, NULL};
//
// A state mechine
// Used to login.
//
// check verify code -> login -> get my info -> get friends
// 		-> get group list name mask -> done.
//
static void qq_loginpanel_login_sm(QQLoginPanel *p, QQCallBack cb
				, gpointer usrdata)
{
	GtkWidget *w;
	w = QQ_LOGINPANEL(usrdata) -> container;
	GtkWidget *dialog, *vc_entry, *img;
	GtkBox *vbox;
	gchar fn[200];
	switch(p -> login_state)
	{
	case LS_CHECK_VC:
		qq_check_verifycode(info, p -> uin, cb, usrdata);
		p -> login_state = LS_LOGIN;
		break;
	case LS_LOGIN:
		if(info -> need_vcimage){
			if(info -> vc_image_data == NULL
					|| info -> vc_image_type == NULL){
				g_warning("No vc image data or type!(%s, %d)"
						, __FILE__, __LINE__);
				p -> login_state = LS_ERROR;
				break;
			}
			sprintf(fn, "/tmp/verifycode.%s"
					, info -> vc_image_type -> str);
			save_img_to_file(info -> vc_image_data -> str
					, info -> vc_image_data -> len
					, "/", fn);
			dialog = gtk_dialog_new_with_buttons("Information"
					, GTK_WINDOW(w), GTK_DIALOG_MODAL
					, GTK_STOCK_OK, GTK_RESPONSE_OK
					, NULL);
			vbox = GTK_BOX(GTK_DIALOG(dialog) -> vbox);
			vc_entry = gtk_entry_new();
			img = gtk_image_new_from_file(fn);
			gtk_box_pack_start(vbox, img, FALSE, FALSE, 0);	
			gtk_box_pack_start(vbox, vc_entry, FALSE, FALSE, 0);	
			gtk_widget_show_all (dialog);
		       	gtk_dialog_run (GTK_DIALOG (dialog));
	       		gtk_widget_destroy (dialog);
		}
		qq_login(info , p -> uin, p -> passwd, p -> status
				, cb, usrdata);
		p -> login_state = LS_GET_MY_INFO;
		break;
	case LS_GET_MY_INFO:
		qq_get_my_info(info, cb, usrdata);
		p -> login_state = LS_GET_FRIENDS;
		break;
	case LS_GET_FRIENDS:
		qq_get_my_friends(info, cb, usrdata);
		p -> login_state = LS_GET_GROUP_LIST;
		break;
	case LS_GET_GROUP_LIST:
		qq_get_group_name_list_mask(info, cb, usrdata);
		p -> login_state = LS_DONE;
		break;
	case LS_DONE:
		qq_mainwindow_show_mainpanel(w);
		break;
	case LS_ERROR:
	case LS_UNKNOWN:
	default:
		break;
	}
}

/*
 * The main event loop context of Gtk.
 */
static GMainContext *gtkctx;

GtkType qq_loginpanel_get_type()
{
	static GType t = 0;
	if(!t){
		static const GTypeInfo info =
			{
				sizeof(QQLoginPanelClass),
				NULL,
				NULL,
				(GClassInitFunc)qq_loginpanelclass_init,
				NULL,
				NULL,
				sizeof(QQLoginPanel),
				0,
				(GInstanceInitFunc)qq_loginpanel_init,
				NULL
			};
		t = g_type_register_static(GTK_TYPE_VBOX, "QQLoginPanel"
						, &info, 0);
	}
	return t;
}

GtkWidget* qq_loginpanel_new(GtkWidget *container)
{
	QQLoginPanel *panel = g_object_new(qq_loginpanel_get_type(), NULL);
	panel -> container = container;

	return GTK_WIDGET(panel);
}

static void qq_loginpanelclass_init(QQLoginPanelClass *c)
{
	GtkObjectClass *object_class = NULL;

	object_class = GTK_OBJECT_CLASS(c);
	object_class -> destroy = qq_loginpanel_destroy;

	/*
	 * get the default main evet loop context.
	 * 
	 * I think this will work...
	 */
	gtkctx = g_main_context_default();
}

/*
 * This is the parameter struct of the XXX_idle function.
 */
typedef struct{
	CallBackResult cbr;
	gpointer redata;
	gpointer usrdata;
}IdlePar;

/*
 * This function is run in the gtk main event loop.
 */
static gboolean gtk_idle(gpointer data)
{
	IdlePar *par = (IdlePar*)data;
	CallBackResult cbr = par -> cbr;
	gpointer redata = par -> redata;
	gpointer usrdata = par -> usrdata;
	g_slice_free(IdlePar, par);

	if(cbr == CB_ERROR){
		g_warning("Error occured!!");
		return FALSE;
	}else if(cbr == CB_SUCCESS){
		//goto next state
		qq_loginpanel_login_sm(QQ_LOGINPANEL(usrdata)
					, login_cb, usrdata);
	}
	return FALSE;
	
}

/*
 * Callback of login function.
 * NOTE:
 * 	This function runs in the different main event loop.
 * 	So, we can NOT change the status of any widgets.
 */
static void login_cb(CallBackResult cbr, gpointer redata, gpointer usrdata)
{
	IdlePar *par = g_slice_new0(IdlePar);
	par -> cbr = cbr;
	par -> redata = redata;
	par -> usrdata = usrdata;
	GSource *src = g_idle_source_new();
	g_source_set_callback(src, (GSourceFunc)gtk_idle, par, NULL);
	g_source_attach(src, gtkctx);
	g_source_unref(src);
}

/*
 * Callback of login_btn button
 */
static void login_btn_cb(GtkButton *btn, gpointer data)
{
	QQLoginPanel *panel = QQ_LOGINPANEL(data);
	GtkWidget *win = QQ_LOGINPANEL(panel) -> container;
	qq_mainwindow_show_splashpanel(win);

	panel -> uin = qq_loginpanel_get_uin(panel);
	panel -> passwd = qq_loginpanel_get_passwd(panel);
	panel -> status = qq_loginpanel_get_status(panel);

	//start the login state mechine
	panel -> login_state = LS_CHECK_VC;
	qq_loginpanel_login_sm(panel, login_cb, data);
}

/*
 * Create the treemode for the status combo box.
 */
static GtkTreeModel* createModel()
{
       	const gchar *files[] = {IMGDIR"status/online.png"
				, IMGDIR"status/hidden.png"
				, IMGDIR"status/away.png"
				, IMGDIR"status/offline.png"};
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	GtkListStore *store;
	gint i;
        
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	for(i = 0; i < 4; i++){
		pixbuf = gdk_pixbuf_new_from_file(files[i], NULL);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, status_label[i], -1);
	}
	return GTK_TREE_MODEL(store);
}

static void qq_loginpanel_init(QQLoginPanel *obj)
{
	obj -> uin_label = gtk_label_new("账  号：");
	obj -> uin_entry = gtk_combo_box_entry_new();

	obj -> passwd_label = gtk_label_new("密  码：");
	obj -> passwd_entry = gtk_entry_new();
	//not visibily 
	gtk_entry_set_visibility(GTK_ENTRY(obj -> passwd_entry), FALSE);
	gtk_widget_set_size_request(obj -> uin_entry, 220, -1);
	gtk_widget_set_size_request(obj -> passwd_entry, 220, -1);
	
	GtkWidget *vbox = gtk_vbox_new(FALSE, 10);

	//uin label and entry
	GtkWidget *uin_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(uin_hbox), obj -> uin_label, FALSE, FALSE, 0);
	GtkWidget *uin_vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(uin_vbox), uin_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(uin_vbox), obj -> uin_entry, FALSE, FALSE, 0);

	//password label and entry
	GtkWidget *passwd_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(passwd_hbox), obj -> passwd_label
					, FALSE, FALSE, 0);
	GtkWidget *passwd_vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(passwd_vbox), passwd_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(passwd_vbox), obj -> passwd_entry, FALSE, FALSE, 0);
	
	//put uin and password in a vbox
	gtk_box_pack_start(GTK_BOX(vbox), uin_vbox, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), passwd_vbox, FALSE, FALSE, 2);

	//rember password check box
	obj -> rempwcb = gtk_check_button_new_with_label("记住密码");
	GtkWidget *hbox4 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox4), obj -> rempwcb, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox4, FALSE, TRUE, 2);

	//login button
	obj -> login_btn = gtk_button_new_with_label("登  录");
	gtk_widget_set_size_request(obj -> login_btn, 90, -1);
	g_signal_connect(G_OBJECT(obj -> login_btn), "clicked"
			, G_CALLBACK(login_btn_cb), (gpointer)obj);

	//status combo box
	obj -> status_comb = gtk_combo_box_new_with_model(createModel());
	gtk_combo_box_set_active(GTK_COMBO_BOX(obj -> status_comb), 0);
	GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(obj -> status_comb)
					, renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(obj -> status_comb), renderer
					,"pixbuf", 0, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(obj -> status_comb)
						, renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(obj -> status_comb), renderer 
					,"text", 1, NULL); 

	GtkWidget *hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox, TRUE, FALSE, 0);

	GtkWidget *hbox2 = gtk_hbox_new(FALSE, 0);
	GtkWidget *hbox3 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), obj -> status_comb, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), obj -> login_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox3), hbox2, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox3, FALSE, FALSE, 20);

	gtk_box_set_homogeneous(GTK_BOX(obj), FALSE);
	GtkWidget *logo = gtk_image_new_from_file(IMGDIR"webqq_icon.png");
	gtk_widget_set_size_request(logo, -1, 150);	
	gtk_box_pack_start(GTK_BOX(obj), logo, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(obj), hbox1, FALSE, FALSE, 15);
}

/*
 * Destroy the instance of QQLoginPanel
 */
static void qq_loginpanel_destroy(GtkObject *obj)
{
	/*
	 * Child widgets will be destroied by their parents.
	 * So, we should not try to unref the Child widgets here.
	 */
}

const gchar* qq_loginpanel_get_uin(QQLoginPanel *loginpanel)
{
	QQLoginPanel *panel = QQ_LOGINPANEL(loginpanel);
	return gtk_combo_box_get_active_text(
				GTK_COMBO_BOX(panel -> uin_entry));	

}
const gchar* qq_loginpanel_get_passwd(QQLoginPanel *loginpanel)
{
	QQLoginPanel *panel = QQ_LOGINPANEL(loginpanel);
	return gtk_entry_get_text(
				GTK_ENTRY(panel -> passwd_entry));
}
const gchar* qq_loginpanel_get_status(QQLoginPanel *loginpanel)
{
	QQLoginPanel *panel = QQ_LOGINPANEL(loginpanel);
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(panel 
							-> status_comb));
	return status[idx];
}