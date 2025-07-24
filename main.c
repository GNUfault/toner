/*
 * Toner - GTK4 and Libadwaita tone generator
 * Copyright (C) 2025  Connor Thomson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
 
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#define SAMPLE_RATE 44100
#define CHUNK_SIZE (SAMPLE_RATE / 10)

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH
} WaveType;

const char *wave_labels[] = { "Sine", "Square", "Triangle", "Sawtooth", NULL };

typedef struct {
    GtkSpinButton *freq_spin;
    GtkDropDown   *wave_dropdown;
    GtkButton     *play_button;
    GtkCssProvider *css_provider;
    GtkWindow     *main_window;
} AppWidgets;

static GThread *play_thread = NULL;
static gboolean keep_playing = FALSE;
static GMutex play_mutex;

static float *generate_wave(WaveType type, double freq, int samples) {
    float *buffer = malloc(sizeof(float) * samples);
    for (int i = 0; i < samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        double phase = fmod(freq * t, 1.0);
        switch (type) {
            case WAVE_SINE:     buffer[i] = sin(2 * M_PI * freq * t); break;
            case WAVE_SQUARE:   buffer[i] = phase < 0.5 ? 1.0 : -1.0; break;
            case WAVE_TRIANGLE: buffer[i] = 4.0 * fabs(phase - 0.5) - 1.0; break;
            case WAVE_SAWTOOTH: buffer[i] = 2.0 * phase - 1.0; break;
        }
    }
    return buffer;
}

static WaveType get_waveform(GtkDropDown *dd) {
    guint index = gtk_drop_down_get_selected(dd);
    return index < 4 ? (WaveType)index : WAVE_SINE;
}

static gboolean restore_play_label(gpointer user_data) {
    AppWidgets *w = (AppWidgets*)user_data;
    gtk_button_set_label(w->play_button, "Play");
    gtk_widget_remove_css_class(GTK_WIDGET(w->play_button), "stop-button");
    return G_SOURCE_REMOVE;
}

static gpointer play_loop(gpointer user_data) {
    AppWidgets *w = user_data;
    double freq = gtk_spin_button_get_value(w->freq_spin);
    WaveType type = get_waveform(w->wave_dropdown);
    
    float *full_buffer = generate_wave(type, freq, SAMPLE_RATE);
    size_t buffer_offset = 0;
    
    pa_sample_spec spec = {
        .format = PA_SAMPLE_FLOAT32LE,
        .rate = SAMPLE_RATE,
        .channels = 1
    };
    int err;
    pa_simple *pa = pa_simple_new(NULL, "ToneGen", PA_STREAM_PLAYBACK, NULL, "play", &spec, NULL, NULL, &err);
    if (!pa) {
        g_printerr("pa_simple_new() failed: %s\n", pa_strerror(err));
        free(full_buffer);
        return NULL;
    }

    while (TRUE) {
        g_mutex_lock(&play_mutex);
        gboolean still_playing = keep_playing;
        g_mutex_unlock(&play_mutex);

        if (!still_playing) {
            break;
        }

        int bytes_to_write = CHUNK_SIZE * sizeof(float);
        if (pa_simple_write(pa, full_buffer + buffer_offset, bytes_to_write, &err) < 0) {
            g_printerr("pa_simple_write() failed: %s\n", pa_strerror(err));
            break;
        }

        buffer_offset = (buffer_offset + CHUNK_SIZE) % SAMPLE_RATE;
    }

    pa_simple_free(pa);
    free(full_buffer);
    g_idle_add(restore_play_label, w);
    return NULL;
}

static void on_play_clicked(GtkButton *btn, gpointer data) {
    AppWidgets *w = data;
    g_mutex_lock(&play_mutex);
    if (!keep_playing) {
        keep_playing = TRUE;
        gtk_button_set_label(btn, "Stop");
        gtk_widget_add_css_class(GTK_WIDGET(btn), "stop-button");
        play_thread = g_thread_new("sound-loop", play_loop, w);
    } else {
        keep_playing = FALSE;
        g_mutex_unlock(&play_mutex);
        g_thread_join(play_thread);
        play_thread = NULL;
        return;
    }
    g_mutex_unlock(&play_mutex);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *win_widget = gtk_application_window_new(app);
    GtkApplicationWindow *win = GTK_APPLICATION_WINDOW(win_widget);
    
    gtk_window_set_title(GTK_WINDOW(win), "Toner");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 240);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), gtk_label_new("Toner"));
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(header_bar), "close:");
    gtk_window_set_titlebar(GTK_WINDOW(win), header_bar);

    GtkWidget *clamp_widget = adw_clamp_new();
    AdwClamp *clamp = ADW_CLAMP(clamp_widget);
    gtk_widget_set_margin_top(clamp_widget, 12);
    gtk_widget_set_margin_bottom(clamp_widget, 24);
    gtk_widget_set_margin_start(clamp_widget, 24);
    gtk_widget_set_margin_end(clamp_widget, 24);
    gtk_window_set_child(GTK_WINDOW(win), clamp_widget);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    adw_clamp_set_child(clamp, vbox);

    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Frequency"));
    GtkSpinButton *spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 1000000.0, 1.0));
    gtk_spin_button_set_value(spin, 440.0);
    gtk_box_append(GTK_BOX(vbox), GTK_WIDGET(spin));

    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Waveform"));
    GtkStringList *model = gtk_string_list_new(wave_labels);
    GtkWidget *dropdown_widget = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    GtkDropDown *dropdown = GTK_DROP_DOWN(dropdown_widget);
    gtk_drop_down_set_selected(dropdown, 0);
    gtk_box_append(GTK_BOX(vbox), dropdown_widget);

    GtkWidget *row_of_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(row_of_buttons, 20);
    gtk_box_append(GTK_BOX(vbox), row_of_buttons);

    GtkWidget *play_btn = gtk_button_new_with_label("Play");
    gtk_box_append(GTK_BOX(row_of_buttons), play_btn);
    gtk_widget_set_hexpand(play_btn, TRUE);
    gtk_widget_set_halign(play_btn, GTK_ALIGN_FILL);
    gtk_widget_add_css_class(play_btn, "thicker-button");


    AppWidgets *w = g_new0(AppWidgets, 1);
    w->freq_spin = spin;
    w->wave_dropdown = dropdown;
    w->play_button = GTK_BUTTON(play_btn);
    w->main_window = GTK_WINDOW(win);

    w->css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(w->css_provider,
                                       ".stop-button { background-color: @error_color; color: @on_error_color; }\n"
                                       "headerbar {\n"
                                       "    background-color: @window_bg_color; /* Match window background */\n"
                                       "    box-shadow: none; /* Remove shadow */\n"
                                       "    border: none; /* Remove any border */\n"
                                       "}\n"
                                       ".thicker-button { padding-top: 20px; padding-bottom: 20px; }\n"
                                       ".headerbar-icon-button {\n"
                                       "    background-color: transparent;\n"
                                       "    border: none;\n"
                                       "    border-image: none;\n"
                                       "    outline: none;\n"
                                       "    box-shadow: none;\n"
                                       "    padding: 4px;\n"
                                       "    min-width: 27px;\n"
                                       "    min-height: 27px;\n"
                                       "    transition: background-color 150ms ease-in-out; /* Smooth transition */\n"
                                       "}\n"
                                       ".headerbar-icon-button:hover {\n"
                                       "    background-color: rgba(0, 0, 0, 0.08);\n"
                                       "    border: none;\n"
                                       "    border-image: none;\n"
                                       "    outline: none;\n"
                                       "    padding: 4px;\n"
                                       "}\n"
                                       ".headerbar-icon-button:active {\n"
                                       "    background-color: rgba(0, 0, 0, 0.15);\n"
                                       "    border: none;\n"
                                       "    border-image: none;\n"
                                       "    outline: none;\n"
                                       "    padding: 4px;\n"
                                       "}\n"
                                      );
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(win_widget),
                                               GTK_STYLE_PROVIDER(w->css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(play_btn, "clicked", G_CALLBACK(on_play_clicked), w);

    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    gtk_init();
    adw_init();
    GtkApplication *app = GTK_APPLICATION(gtk_application_new("com.connor.wavegen", G_APPLICATION_DEFAULT_FLAGS));
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
