#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <string.h>

#include "../include/http.h"
#include "../include/config.h"
#include "../include/external.h"

static gboolean manual_entry = FALSE; // Flag untuk menandai input manual

void load_page(const char *url, WebKitWebView *web_view) {
    if (strncmp(url, "http://", 7) == 0) {
        char *url_copy = strdup(url);

        char *response = handle_response(url_copy, "");

        // Dapatkan HTML yang dimodifikasi dari find_external_resources
        find_external_resources(response, url_copy);
        webkit_web_view_load_html(web_view, response, url_copy);

        free(response);

        free(url_copy);
    }
    else if(strncmp(url, "https://", 8) == 0) {
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), url); // Load URL ke WebView
    } else {
        g_print("URL tidak valid. Harus diawali dengan http:// atau https://\n");
    }
}

void on_activate_entry(GtkEntry *entry, WebKitWebView *web_view, GtkWidget *spinner) {
    const char *url = gtk_entry_get_text(entry);

    if (url && strlen(url) > 0) {
        manual_entry = TRUE; // Set flag untuk menandai input manual
        load_page(url, web_view);
    }
}

void on_uri_changed(GObject *web_view, GParamSpec *pspec, GtkEntry *entry, GtkWidget *spinner) {
    const char *current_url = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(web_view));

    // Hanya proses URL jika bukan dari input manual atau pertama kali masuk
    if (!manual_entry) {
        gtk_entry_set_text(entry, current_url); // Perbarui GtkEntry dengan URL baru
        //printf("Navigasi otomatis URL: %s\n", current_url);
    }

    // Reset flag manual_entry setelah URL diproses dari input manual
    manual_entry = FALSE;
}

// Fungsi untuk mrnjalankan javascript
static void on_javascript_finished(WebKitWebView *web_view, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    WebKitJavascriptResult *js_result = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(web_view), res, &error);

    if (error) {
        g_printerr("Error menjalankan JavaScript: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    // Konversi WebKitJavascriptResult ke JSCValue
    JSCValue *result = webkit_javascript_result_get_js_value(js_result);
    if (result && jsc_value_is_string(result)) {
        char *form_data = jsc_value_to_string(result);
        //g_print("Form Data:%s\n", form_data);
        // Kirim request POST ke server
        const char *url = (const char *)user_data;
        char *url_copy = strdup(url);
        char *response = handle_response(url_copy, form_data);

        // Dapatkan HTML yang dimodifikasi dari find_external_resources
        find_external_resources(response, url_copy);
        webkit_web_view_load_html(web_view, response, url_copy);
        g_free(form_data);
        free(response);
    }

    // Bersihkan objek
    webkit_javascript_result_unref(js_result);
}

//Fungsi untuk menangani click hyperlink (a href)
gboolean on_decide_policy(
    WebKitWebView *web_view, 
    WebKitPolicyDecision *decision, 
    WebKitPolicyDecisionType type, 
    gpointer user_data) {
    
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);

        WebKitNavigationType navigation_type = webkit_navigation_action_get_navigation_type(action);

        if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED) {
            const char *url = webkit_uri_request_get_uri(request);
            GtkEntry *entry = GTK_ENTRY(user_data);

            // Update GtkEntry dan set URL baru
            gtk_entry_set_text(entry, url);

            // Cegah navigasi default untuk klik link
            webkit_policy_decision_ignore(decision);

            // Set flag manual_entry ke FALSE karena ini dari hyperlink
            manual_entry = FALSE;

            // Muat URL ke WebView
            //printf("Muat URL dari on_decide_policy : %s\n", url);
            load_page(url, web_view);
            return TRUE;
        }

        const gchar *method = webkit_uri_request_get_http_method(request);
        if (method && g_strcmp0(method, "POST") == 0) {
            // Handle POST request
            const gchar *url = webkit_uri_request_get_uri(request);
            GtkEntry *entry = GTK_ENTRY(user_data);

            // Set URL in the entry for display
            gtk_entry_set_text(entry, url);

            // Ignore the navigation to prevent the WebView from handling it
            webkit_policy_decision_ignore(decision);

            // Run custom JavaScript to gather form data for POST
            const gchar *js_code = 
                "let formData = new FormData(document.querySelector('form'));"
                "let data = '';"
                "for (let [key, value] of formData.entries()) {"
                "  data += encodeURIComponent(key) + '=' + encodeURIComponent(value) + '&';"
                "}"
                "data.slice(0, -1);";  // remove trailing "&"

            webkit_web_view_run_javascript(web_view, js_code, NULL, (GAsyncReadyCallback)on_javascript_finished, (gpointer)url);
            return TRUE;
        }
    }
    return FALSE; // Biarkan WebView memuat URL jika bukan klik link
}

void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data) {
    GtkWidget *spinner = GTK_WIDGET(user_data);
    
    if (load_event == WEBKIT_LOAD_STARTED) {
        // Tampilkan spinner saat halaman mulai dimuat
        gtk_spinner_start(GTK_SPINNER(spinner));
        gtk_widget_show(spinner);
    } else if (load_event == WEBKIT_LOAD_FINISHED) {
        // Sembunyikan spinner setelah loading selesai
        gtk_spinner_stop(GTK_SPINNER(spinner));
        gtk_widget_hide(spinner);
    }
}

void display_html() {
    // Inisialisasi GTK
    gtk_init(NULL, NULL);

    /*g_object_set(
        gtk_settings_get_default(), 
        "gtk-overlay-scrolling", 
        FALSE, NULL);
    */
    // Buat window utama
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
    // Dapatkan ukuran layar
    GdkMonitor *monitor = gdk_display_get_primary_monitor(gdk_display_get_default());
    GdkRectangle monitor_geometry;
    gdk_monitor_get_geometry(monitor, &monitor_geometry);

    // Dapatkan ukuran monitor dalam piksel
    int screen_width = monitor_geometry.width;
    int screen_height = monitor_geometry.height;

    gtk_window_set_default_size(GTK_WINDOW(window), screen_width, screen_height);
    gtk_window_set_title(GTK_WINDOW(window), "My Browser");

    // Buat widget WebKit untuk menampilkan halaman web
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    // Buat entry (textbox) untuk input URL
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Isi URL dan tekan Enter");

    // Buat spinner untuk loading
    GtkWidget *spinner = gtk_spinner_new();
    gtk_widget_set_size_request(spinner, 20, 20); 
    gtk_widget_hide(spinner); 

    // Hubungkan sinyal "activate" (tekan Enter) pada gtk_entry
    g_signal_connect(entry, "activate", G_CALLBACK(on_activate_entry), web_view);

    // Hubungkan sinyal untuk meng-update text entry ketika ada perubahan URL
    g_signal_connect(web_view, "notify::uri", G_CALLBACK(on_uri_changed), entry);

    // Hubungkan sinyal untuk memproses event klik hyperlink
    g_signal_connect(web_view, "decide-policy", G_CALLBACK(on_decide_policy), entry);

    // Hubungkan sinyal untuk spinner
    g_signal_connect(web_view, "load-changed", G_CALLBACK(on_load_changed), spinner);

    // Buat container horizontal untuk menempatkan entry dan spinner
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0); // Entry mengambil ruang
    gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0); // Spinner tidak mengambil ruang ekstra

    // Buat container vertical untuk mengatur posisi hbox dan WebView
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); // Tambahkan hbox ke vbox
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(web_view), TRUE, TRUE, 0);

    // Tambahkan vbox ke window
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Tampilkan semua widget di window
    gtk_widget_show_all(window);

    // Tutup program saat window ditutup
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Mulai loop GTK
    gtk_main();
}

int main(int argc, char *argv[]) {
    load_config("javanese_browser.conf");
    display_html();
    return 0;
}