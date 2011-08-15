#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define MIN_DESKTOPS 2
#define MAX_DESKTOPS 9

Display  *dpy;
Window   root;

Atom NET_NUMBER_OF_DESKTOPS;
Atom NET_CLIENT_LIST;
Atom NET_WM_DESKTOP;

unsigned long* xprop_get_data(Window win, Atom property, Atom type, int *items)
{
    Atom type_ret;
    int format_ret;
    unsigned long items_ret;
    unsigned long after_ret;
    unsigned long *prop_data;

    prop_data = 0;

    XGetWindowProperty(dpy, win, property, 0, 0x7fffffff,
                       false, type, &type_ret, &format_ret, &items_ret,
                       &after_ret, &prop_data);
    if (items)
        *items = items_ret;

    return prop_data;
}

void init_atoms()
{
    NET_CLIENT_LIST = XInternAtom(dpy, "_NET_CLIENT_LIST", false);
    NET_NUMBER_OF_DESKTOPS = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", false);
    NET_WM_DESKTOP = XInternAtom(dpy, "_NET_WM_DESKTOP", false);     
}

bool open_display()
{
    int num;

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return false;

    num = DefaultScreen(dpy);
    root = RootWindow(dpy, num);

    XSelectInput(dpy, root, PropertyChangeMask | SubstructureNotifyMask);
    XSync(dpy, false);
    XFlush(dpy);

    return true;
}

void set_number_of_desktops(long n)
{
    XEvent event;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = NET_NUMBER_OF_DESKTOPS;
    event.xclient.window = root;
    event.xclient.format = 32;
    event.xclient.data.l[0] = n;
    event.xclient.data.l[1] = 0;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;
    XSendEvent(dpy, root, False, mask, &event);
}

void modify_desktops(bool last, bool prev, int desks)
{
    if(last && desks < MAX_DESKTOPS){
        set_number_of_desktops(desks + 1);
        return;
    }
    if(!last && !prev && desks > MIN_DESKTOPS){
        set_number_of_desktops(desks - 1);
        return;
    }
}


void iterate_client_list(int number_of_desktops)
{
    int last_desktop = number_of_desktops - 1;

    bool win_on_last = false;
    bool win_on_prev  = false;

    Window *client_list;
    int client_list_size;

    client_list = xprop_get_data(root, NET_CLIENT_LIST, XA_WINDOW, &client_list_size);
    
    if (!client_list) {
        // client list is empty
        return;
    }

    if (client_list == NULL) {
        printf("Cannot get client list.\n");
        exit(1);
    }

    for (int i = client_list_size - 1; i >= 0; i--){
        signed long *desktop;
        desktop = (signed long *) xprop_get_data(client_list[i],
                                    NET_WM_DESKTOP, XA_CARDINAL, 0);
        if (*desktop == last_desktop){
            win_on_last = true;
        }
        if (*desktop == last_desktop -1){
            win_on_prev = true;
        }
        XFree(desktop);
    }
    
    modify_desktops(win_on_last, win_on_prev, number_of_desktops);

    XFree(client_list);
}


int get_number_of_desktops()
{
    int num; 
    unsigned long *data;

    data = 0;
    data = xprop_get_data(root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 0);
    
    if (data) {
        num = *data;
        XFree(data);
    } else {
        printf("Cannot get number of desktops\n");
        exit(1);
    }

    return num;
}

void event_loop()
{
    XEvent ev;
    int xfd;

    xfd = ConnectionNumber(dpy);

    while(1){
        fd_set fd;

        FD_ZERO(&fd);
        FD_SET(xfd, &fd);
        select(xfd + 1, &fd, 0, 0, 0);

        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if(ev.type == PropertyNotify) {
                iterate_client_list(get_number_of_desktops());
            }        
        }
    }
}

int main(int argc, char *argv[])
{
    if (!open_display()) {
        printf("Cannot open display\n");
        return 1;
    }

    init_atoms();

    iterate_client_list(get_number_of_desktops());
    event_loop();

    XCloseDisplay (dpy);
    return 0;
}
