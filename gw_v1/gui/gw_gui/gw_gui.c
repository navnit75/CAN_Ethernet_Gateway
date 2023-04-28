#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h> 

GtkWidget *rtextview;
GtkWidget *stextview;
GtkWidget *ecu1_tview;
GtkWidget *ecu2_tview;
GtkWidget *ecu3_tview;
GtkWidget *ecu4_tview;
GtkWidget *ecu1_rt_tview;
GtkWidget *ecu2_rt_tview;
GtkWidget *ecu3_rt_tview;
GtkWidget *ecu4_rt_tview;


GtkWidget *radiobutton_udp;
GtkWidget *radiobutton_tcp;
GtkWidget *radiobutton_none;


void refresh_from_ecu_file(char *fname,GtkWidget *ecu_tview) 
{
    FILE *fptr;
    GtkTextBuffer *buffer;
    GtkTextIter s, e;
    char buf[4098];
    // Open one file for reading
    fptr = fopen(fname, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file %s \n",fname);
        exit(0);
    }
 // fread(&num, sizeof(struct threeNum), 1, fptr);
//  fwrite(&num, sizeof(struct threeNum), 1, fptr);
    memset(buf,'\0',4098); 
    fread(buf, 4098, 1, fptr);  
    printf("File: %s \n String : %s \n",fname,buf);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ecu_tview));
    gtk_text_buffer_get_iter_at_offset (buffer, &s, 0);
    gtk_text_buffer_get_iter_at_offset (buffer, &e, -1);
    gtk_text_buffer_delete(buffer,&s,&e);

    gtk_text_buffer_set_text (buffer, buf, -1);
    gtk_widget_show(ecu_tview);
    fclose(fptr);
    //return 0;
}

/* Function added to read the config.txt  file */ 
void read_from_conf_file(char *fname,char *buffer){
    FILE *fptr; 
    fptr = fopen(fname,"r");
    if(fptr == NULL){
        printf("Cannot open file %s \n",fname);
        exit(0); 
    }

    fread(buffer,sizeof(char),4,fptr);
    g_print(" Connection Type: %s\n", buffer);
    fclose(fptr);
}

/************************************************/
/* Function added to read the config.json file */
void read_from_conf_json_file(char *fname, char *non_critical, char *critical1, char *critical2){
    
    /* buffer for json read */ 
    char buffer_json[1024];

    /* temp struct to store the file and parsed file */ 
    struct json_object *parsed_json; 
    struct json_object *connection_type; 
    struct json_object *connection_type_critical1; 
    struct json_object *connection_type_critical2; 
    
    /* reading the file */ 
    FILE *fptr; 
    fptr = fopen(fname,"r");
    if(fptr == NULL){
        printf("Cannot open file %s \n",fname);
        exit(0); 
    }
    fread(buffer_json,1024,1,fptr);
    fclose(fptr);


    /* parse the data read from file */ 
    parsed_json = json_tokener_parse(buffer_json);
    json_object_object_get_ex(parsed_json,"connection_type",&connection_type);
    json_object_object_get_ex(parsed_json,"interface_1_eth2can",&connection_type_critical1);
    json_object_object_get_ex(parsed_json,"interface_2_can2eth",&connection_type_critical2);

    /* copy into provided arguments */ 
    memcpy(non_critical,json_object_get_string(connection_type),5);
    memcpy(critical1,json_object_get_string(connection_type_critical1),5);
    memcpy(critical2,json_object_get_string(connection_type_critical2),5);
    /* print for verification */ 
    g_print(" Connection Type: %s\n", non_critical);
    g_print(" critical 1 (eth2can) : %s\n", critical1);
    g_print(" critical 2 (can2eth) : %s\n", critical2);
}
/**************************************************/

/***********************************************************************/
void refresh_from_ecu_file_text(char *fname,GtkWidget *ecu_tview) 
{
        FILE *fptr;
        GtkTextBuffer *buffer;
        GtkTextIter s, e;
        char buf[4098];
        long len = 0;
        // Open one file for reading
        fptr = fopen(fname, "r");
        if (fptr == NULL)
        {
                printf("Cannot open file %s \n",fname);
                exit(0);
        }
        fseek(fptr, 0, SEEK_END);
        len = ftell(fptr);
        fseek(fptr, 0, SEEK_SET);
        if(len > 4098)
        {
                fseek(fptr, -4098, SEEK_END);
        }

        // fread(&num, sizeof(struct threeNum), 1, fptr);
        //  fwrite(&num, sizeof(struct threeNum), 1, fptr);
        memset(buf,'\0',4098);

        fread(buf, 4098, 1, fptr);
        printf("File: %s \n String : %s \n",fname,buf);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ecu_tview));
        gtk_text_buffer_get_iter_at_offset (buffer, &s, 0);
        gtk_text_buffer_get_iter_at_offset (buffer, &e, -1);
        gtk_text_buffer_delete(buffer,&s,&e);

        gtk_text_buffer_set_text (buffer, buf, -1);
        gtk_widget_show(ecu_tview);
        fclose(fptr);
        //return 0;
}
/***********************************************************************/

void refresh_from_recv_file(void) 
{
    refresh_from_ecu_file_text("/opt/gateway/can1.txt",ecu1_tview);
    refresh_from_ecu_file_text("/opt/gateway/can2.txt",ecu2_tview);
    refresh_from_ecu_file_text("/opt/gateway/can3.txt",ecu3_tview);
    refresh_from_ecu_file_text("/opt/gateway/can_tscf.txt",ecu4_tview);

    refresh_from_ecu_file("/opt/gateway/can6_rt.txt",ecu1_rt_tview);
    refresh_from_ecu_file("/opt/gateway/can7_rt.txt",ecu2_rt_tview);
    refresh_from_ecu_file("/opt/gateway/can8_rt.txt",ecu3_rt_tview);
    refresh_from_ecu_file("/opt/gateway/can_c_rt.txt",ecu4_rt_tview);
    

}


void update_send_file(char *str) 
{
    FILE *fptr;
    char buf[2048];
    char *pbuf = buf;
    GtkTextBuffer *buffer;
    GtkTextIter s, e;
    // Open one file for reading
    fptr = fopen("/opt/gateway/send.txt", "w");
    if (fptr == NULL)
    {
        printf("Cannot open file /opt/gateway/send.txt \n");
        exit(0);
    }
 // fread(&num, sizeof(struct threeNum), 1, fptr);
//  fwrite(&num, sizeof(struct threeNum), 1, fptr);
    printf("String : %s and size: %ld \n",str,strlen(str)); 
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (stextview));

gtk_text_buffer_get_iter_at_offset (buffer, &s, 0);
gtk_text_buffer_get_iter_at_offset (buffer, &e, -1);

   // gtk_text_buffer_get_bounds (buffer, &s, &e);
    pbuf = (char *)gtk_text_buffer_get_text (buffer, &s,&e, FALSE);
    printf("Buffer: %s \n",pbuf);
    fwrite(str, strlen(str), 1, fptr);
    fwrite(pbuf, strlen(pbuf), 1, fptr);  
    fclose(fptr);
printf(" \n\n");
    //return 0;
}

void update_config_file(char *str) 
{
    FILE *fptr;
    // Open one file for reading
    fptr = fopen("/opt/gateway/conf.txt", "w");
    if (fptr == NULL)
    {
        printf("Cannot open file /opt/gateway/conf.txt \n");
        exit(0);
    }
    // fread(&num, sizeof(struct threeNum), 1, fptr);
    //  fwrite(&num, sizeof(struct threeNum), 1, fptr);
    printf("String : %s and size: %ld \n",str,strlen(str)); 
    fwrite(str, 3, 1, fptr);  
    fclose(fptr);
    //return 0;
}


static void destroy(GtkWidget *widget, gpointer data)
{
    remove("/opt/gateway/can1.txt");
    remove("/opt/gateway/can2.txt");
    remove("/opt/gateway/can3.txt");
    remove("/opt/gateway/can_tscf.txt");
    remove("/opt/gateway/can6_rt.txt");
    remove("/opt/gateway/can7_rt.txt");
    remove("/opt/gateway/can8_rt.txt");
    remove("/opt/gateway/can_c_rt.txt");
    remove("/opt/gateway/send.txt");
    gtk_main_quit();
}

static void send_clicked(GtkWidget *button, gpointer data)
{
    g_print("send clicked\n");
    update_send_file("Send Once\n");
}

static void recv_clicked(GtkWidget *button, gpointer data)
{
    g_print("recv clicked\n");
    refresh_from_recv_file();
}

static void csend_clicked(GtkWidget *button, gpointer data)
{
    g_print("csend clicked\n");
    update_send_file("Send Continuous\n");
}

static void crecv_clicked(GtkWidget *button, gpointer data)
{
    g_print("crecv clicked\n");
    FILE *fptr; 
    fptr = fopen("/opt/gateway/can6_rt.txt","w");
    fclose(fptr);
    fptr = fopen("/opt/gateway/can7_rt.txt","w");
    fclose(fptr);
    fptr = fopen("/opt/gateway/can8_rt.txt","w");
    fclose(fptr);
    fptr = fopen("/opt/gateway/can_c_rt.txt","w");
    fclose(fptr);
    refresh_from_recv_file();
}

/* Radio Button toggle event handler */ 
static void radio_button_toggled(GtkWidget *radiobutton, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton)))
    {
        
        /* If a user selects any of the option DISABLE 'None' radiobutton */
        gtk_widget_set_sensitive (radiobutton_none, FALSE);
        
        /* Print the terminal log */ 
        g_print("%s active\n", gtk_button_get_label(GTK_BUTTON(radiobutton)));

        /* Update the config file on toggle */ 
	    update_config_file((char *)gtk_button_get_label(GTK_BUTTON(radiobutton)));


        /* If udp is selected disable the tcp radiobutton */ 
        if(radiobutton == radiobutton_udp)
                    gtk_widget_set_sensitive (radiobutton_tcp, FALSE);

        /* If tcp is selected disable the udp radiobutton */ 
        else if(radiobutton == radiobutton_tcp) 
                    gtk_widget_set_sensitive (radiobutton_udp, FALSE);
    }

}

int Create_Radio_Button(GtkWidget *fixed)
{

   // GtkWidget *box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 5);
   // gtk_fixed_put(GTK_FIXED(fixed), box, 15, 15);	
   // gtk_container_add(GTK_CONTAINER(window), box);

   /* Creates a new radio button */ 
    radiobutton_udp = gtk_radio_button_new_with_label(NULL, "UDP");
    
    /* Connecting a call back */ 
    g_signal_connect(radiobutton_udp, "toggled", G_CALLBACK(radio_button_toggled), NULL);
    // gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(radiobutton), FALSE, FALSE, 0);
    
    /* Positioning the radio button */ 
    gtk_fixed_put(GTK_FIXED(fixed), radiobutton_udp, 15, 30);    
    

    /* Create a group pointer */ 
    GSList *group;

    /* Get the group of above radio button created */
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiobutton_udp));

    /* Create a new radio button in the same group */ 
    radiobutton_tcp = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiobutton_udp)), "TCP");
    
    /* Connecting the signal */
    g_signal_connect(radiobutton_tcp, "toggled", G_CALLBACK(radio_button_toggled), NULL);
    //gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(radiobutton), FALSE, FALSE, 0);
    
    /* Positioning the radio button */
    gtk_fixed_put(GTK_FIXED(fixed), radiobutton_tcp, 80, 30);  

    /* Create another radio button */
    radiobutton_none = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiobutton_udp)), "None");
    
    /* Enabling the "None" now */ 
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton_none), TRUE);

    /* Connecting the signal */
    g_signal_connect(radiobutton_none, "toggled", G_CALLBACK(radio_button_toggled), NULL);
    //gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(radiobutton), FALSE, FALSE, 0);
    
    /* Positioning the radio button */
    gtk_fixed_put(GTK_FIXED(fixed), radiobutton_none, 150, 30); 


}

GtkWidget* Create_TextEntry(GtkWidget *fixed, int x, int y, int w, int h)  //rstext =1(Send),2(Recv) 
{
    GtkWidget *textview;
    GtkTextBuffer *buffer;
    GtkTextIter start, end;
    GdkRGBA rgba;
    GtkTextTag *tag;
    GtkCssProvider *provider;
    GtkStyleContext *context;    
//    int y = 50;
//    int x = 15;
//    int w = 200;
//    int h = 200;	

textview = gtk_text_view_new();

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  //  gtk_text_buffer_set_text (buffer, "Hello \n Hhh \n Hello \n Hhh \n Hello \n Hhh \n Hello \n Hhh ", -1);

GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL,NULL);
gtk_widget_set_size_request(scrolledwindow, w, h);
gtk_container_add(GTK_CONTAINER(scrolledwindow), textview);
gtk_fixed_put(GTK_FIXED(fixed), scrolledwindow, x, y);

   // gtk_fixed_put(GTK_FIXED(fixed), textview, x, 80);
    gtk_widget_set_size_request(textview, w, h);	
/* Change default font and color throughout the widget */
provider = gtk_css_provider_new ();
gtk_css_provider_load_from_data (provider,
                                 "textview {"
                                 "  font: 15 serif;"
                                 "  color: green;"
                                 "}",
                                 -1,
                                 NULL);
context = gtk_widget_get_style_context (textview);
gtk_style_context_add_provider (context,
                                GTK_STYLE_PROVIDER (provider),
                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

/* Change left margin throughout the widget */
//gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 30);

/* Use a tag to change the color for just one part of the widget */
tag = gtk_text_buffer_create_tag (buffer,
                                  "blue_foreground",
                  "foreground", "blue",
                  NULL);  
//gtk_text_buffer_get_iter_at_offset (buffer, &start, 7);
//gtk_text_buffer_get_iter_at_offset (buffer, &end, 12);
//gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

   // gtk_container_add(GTK_CONTAINER(window), textview);
   return textview;

}

void create_send_recv_buttons(GtkWidget *fixed) 
{
 GtkWidget *button;
    
    button = gtk_button_new_with_label("Send once");
    g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(send_clicked), NULL);
    
    gtk_fixed_put(GTK_FIXED(fixed), button, 15, 620);
    gtk_widget_set_size_request(button, 80, 30);


    button = gtk_button_new_with_label("Receive once");
    g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(recv_clicked), NULL);
    
    gtk_fixed_put(GTK_FIXED(fixed), button, 220, 620);
    gtk_widget_set_size_request(button, 80, 30);

    button = gtk_button_new_with_label("Send Continuous");
    g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(csend_clicked), NULL);
    
    gtk_fixed_put(GTK_FIXED(fixed), button, 15, 650);
    gtk_widget_set_size_request(button, 80, 30);


    button = gtk_button_new_with_label("Receive Continuous");
    g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(crecv_clicked), NULL);
    gtk_fixed_put(GTK_FIXED(fixed), button, 220, 650);
    gtk_widget_set_size_request(button, 80, 30);
	
}
int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *textview;
    GtkTextBuffer *buffer;
    GtkTextIter start, end;
    GdkRGBA rgba;
    GtkTextTag *tag;
    GtkCssProvider *provider;
    GtkStyleContext *context;    
    GtkWidget *fixed;
    GtkWidget *label;
	
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gateway-CAN ECU");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 800);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
  
  //  GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
  //  gtk_container_add(GTK_CONTAINER(window), scrolledwindow);
    
    
    
    fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    //gtk_container_add(GTK_CONTAINER(scrolledwindow), fixed);    

    // label = gtk_label_new("Static configuration");	  
    // gtk_fixed_put(GTK_FIXED(fixed), label, 10, 5);    

     
	
    // Create_Radio_Button(fixed);


    char conf_read_data[5];
    char conf_read_data1[5];
    char conf_read_data2[5];
    
    read_from_conf_json_file("/opt/gateway/conf.json",conf_read_data,conf_read_data1,conf_read_data2);

    label = gtk_label_new("Connection Type ( non-critical ): "); 
    gtk_fixed_put(GTK_FIXED(fixed), label, 10, 10); 
    label = gtk_label_new(conf_read_data);
    gtk_fixed_put(GTK_FIXED(fixed), label, 225,10);

    label = gtk_label_new("Connection Type ( critical (eth2can)): "); 
    gtk_fixed_put(GTK_FIXED(fixed), label, 10, 35);    
    label = gtk_label_new(conf_read_data1);
    gtk_fixed_put(GTK_FIXED(fixed), label, 260,35);

    label = gtk_label_new("Connection Type ( critical (can2eth)): "); 
    gtk_fixed_put(GTK_FIXED(fixed), label, 400, 35);    
    label = gtk_label_new(conf_read_data2);
    gtk_fixed_put(GTK_FIXED(fixed), label, 650, 35);


    label = gtk_label_new("Dynamic control");	
    gtk_fixed_put(GTK_FIXED(fixed), label, 10, 70);

    label = gtk_label_new("Send text(CAN-->ETH)");	
    gtk_fixed_put(GTK_FIXED(fixed), label, 10, 100);

    label = gtk_label_new("Received text(ETH ECU)");	
    gtk_fixed_put(GTK_FIXED(fixed), label, 250, 100);

    label = gtk_label_new("Round trip time(usecs)[CAN/GW-->ETH-->GW/CAN]");	
    gtk_fixed_put(GTK_FIXED(fixed), label, 700, 100);

    
    stextview = Create_TextEntry(fixed,15,140,200,200);
    
    ecu1_tview = Create_TextEntry(fixed,220,140,400,100);
    ecu2_tview = Create_TextEntry(fixed,220,260,400,100);
    ecu3_tview = Create_TextEntry(fixed,220,380,400,100);
    ecu4_tview = Create_TextEntry(fixed,220,500,400,100);

    ecu1_rt_tview = Create_TextEntry(fixed,650,140,400,100);
    ecu2_rt_tview = Create_TextEntry(fixed,650,260,400,100);
    ecu3_rt_tview = Create_TextEntry(fixed,650,380,400,100);
    ecu4_rt_tview = Create_TextEntry(fixed,650,500,400,100);
    create_send_recv_buttons(fixed);	
    
    //gtk_container_add(GTK_CONTAINER(window), button);
    
    gtk_widget_show_all(window);
    
    gtk_main();
    
    return 0;
}
