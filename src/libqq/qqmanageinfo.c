#ifndef __LIBQQ_QQ_MANAGEINFO_H
#define __LIBQQ_QQ_MANAGEINFO_H
/*
 * Manage the information of the user
 *
 * 	1.Get friends' information.
 * 	2.Get friends' pictures.
 * 	3.Get groups' informatin.
 * 	4.Get self information.
 * 	5.Manage all above.
 */

#include <qq.h>
#include <qqtypes.h>
#include <url.h>
#include <http.h>
#include <json.h>
#include <qqhosts.h>

static gboolean do_get_my_info(gpointer data)
{
	DoFuncParam *par = (DoFuncParam*)data;
	if(par == NULL){
		g_warning("par == NULL in do_get_my_info.(%s, %d)", __FILE__
				, __LINE__);
		return FALSE;
	}
	QQInfo *info = par -> info;
	QQCallBack cb = par -> cb;
	g_free(par);

	JSON *json = JSON_new();
	gchar params[300];
	g_debug("Get my information!(%s, %d)", __FILE__, __LINE__);

	Request *req = request_new();
	Response *rps = NULL;
	request_set_method(req, "GET");
	request_set_version(req, "HTTP/1.1");
	g_sprintf(params, GETMYINFO"?tuin=%s&verifysession=&code=&"
			"vfwebqq=%s&t=%lld"
			, info -> me -> uin -> str
			, info -> vfwebqq -> str, get_now_millisecond());
	request_set_uri(req, params);
	request_set_default_headers(req);
	request_add_header(req, "Host", SWQQHOST);
	request_add_header(req, "Cookie", info -> cookie -> str);
	request_add_header(req, "Content-Type", "utf-8");
	request_add_header(req, "Referer"
			, "http://s.web2.qq.com/proxy.html?v=20101025002");

	Connection *con = connect_to_host(SWQQHOST, 80);
	if(con == NULL){
		g_warning("Can NOT connect to server!(%s, %d)"
				, __FILE__, __LINE__);
		if(cb != NULL){
			cb(CB_NETWORKERR, "Can not connect to server!");
		}
		request_del(req);
		g_free(par);
		return FALSE;
	}

	send_request(con, req);
	rcv_response(con, &rps);
	close_con(con);
	connection_free(con);

	const gchar *retstatus = rps -> status -> str;
	if(g_strstr_len(retstatus, -1, "200") == NULL){
		/*
		 * Maybe some error occured.
		 */
		g_warning("Resoponse status is NOT 200, but %s (%s, %d)"
				, retstatus, __FILE__, __LINE__);
		if(cb != NULL){
			cb(CB_ERROR, "Response error!");
		}
		goto error;
	}

	g_printf("(%s, %d)%s\n", __FILE__, __LINE__, rps -> msg -> str);
	JSON_set_raw_data(json, rps -> msg);
	JSON_parse(json);
	JSON_print(json);

	GString *val;
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "nick");
	if(val != NULL){
		g_debug("nick: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> nick = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "country");
	if(val != NULL){
		g_debug("country: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> country = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "province");
	if(val != NULL){
		g_debug("province: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> province = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "city");
	if(val != NULL){
		g_debug("city: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> city = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "gender");
	if(val != NULL){
		g_debug("gender: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> gender = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "phone");
	if(val != NULL){
		g_debug("phone: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> phone = val;
	}
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "mobile");
	if(val != NULL){
		g_debug("mobile: %s (%s, %d)", val -> str, __FILE__, __LINE__);
		info -> me -> mobile = val;
	}

	GString *year, *month, *day;
	year = (GString *)JSON_find_pair_value(json, JSON_STRING, "year");
	month = (GString *)JSON_find_pair_value(json, JSON_STRING, "month");
	day = (GString *)JSON_find_pair_value(json, JSON_STRING, "day");
	if(year != NULL && month != NULL && day != NULL){
		g_debug("year:%smonth:%sday:%s", year -> str, month -> str
				, day -> str);
		gint tmpi;
		gchar *endptr;
		tmpi = strtol(year -> str, &endptr, 10);
		if(endptr == year -> str){
			g_warning("strtol error. input: %s (%s, %d)"
					, year -> str, __FILE__, __LINE__);
		}else{
			info -> me -> birthday.year = tmpi;
		}
		tmpi = strtol(month -> str, &endptr, 10);
		if(endptr == month -> str){
			g_warning("strtol error. input: %s (%s, %d)"
					, month -> str, __FILE__, __LINE__);
		}else{
			info -> me -> birthday.month = tmpi;
		}
		tmpi = strtol(day -> str, &endptr, 10);
		if(endptr == day -> str){
			g_warning("strtol error. input: %s (%s, %d)"
					, day -> str, __FILE__, __LINE__);
		}else{
			info -> me -> birthday.day = tmpi;
		}
		g_debug("Birthday:%d/%d/%d", info -> me -> birthday.year
				, info -> me -> birthday.month
				, info -> me -> birthday,day);
	}

	/*
	 * Just to check error.
	 */
	val = (GString *)JSON_find_pair_value(json, JSON_STRING, "uin");
	if(val == NULL){
		g_debug("(%s, %d) %s", __FILE__, __LINE__, rps -> msg -> str);
	}
error:
	JSON_free(json);
	request_del(req);
	response_del(rps);
	return FALSE;
}

void qq_get_my_info(QQInfo *info, QQCallBack cb)
{
	if(info == NULL){
		if(cb != NULL){
			cb(CB_ERROR, "info == NULL in qq_get_my_info");
		}
		return;
	}
	GSource *src = g_idle_source_new();
	DoFuncParam *par = g_malloc(sizeof(*par));
	par -> info = info;
	par -> cb = cb;
	g_source_set_callback(src, (GSourceFunc)do_get_my_info, (gpointer)par, NULL);
	if(g_source_attach(src, info -> mainctx) <= 0){
		g_error("Attach logout source error.(%s, %d)"
				, __FILE__, __LINE__);
	}
	g_source_unref(src);
	return;
}

#endif