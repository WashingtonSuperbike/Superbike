# LVGL Data-Driven UI Guide for This Example

This example is a good starting point for building a custom graphical interface where the screen is driven by variables stored on the device. A car dashboard is a strong fit for this pattern because most of the UI is just a live view of state: speed, RPM, fuel, battery, temperature, gear, warnings, and touch controls.

The code in this folder already does the hard part of bringing up the display and LVGL. Your job is to replace the demo widgets with your own objects and feed them with real data.

## What This Example Already Does

The sketch initializes the board, starts the LVGL port, and creates a demo UI:

- [09_lvgl_Porting.ino](09_lvgl_Porting.ino) creates the board, calls `board->init()`, then starts LVGL with `lvgl_port_init(board->getLCD(), board->getTouch())`.
- [lvgl_v8_port.h](lvgl_v8_port.h) exposes the LVGL port helpers, including `lvgl_port_init()`, `lvgl_port_lock()`, and `lvgl_port_unlock()`.
- [lvgl_v8_port.cpp](lvgl_v8_port.cpp) registers the display driver, optionally registers the touch driver, and starts the LVGL task.

Right now the sketch ends by calling `lv_demo_widgets()`. That is the line you would replace with your own dashboard UI.

## The Basic Pattern

For a data-driven interface, keep the UI and the data model separate:

1. Store live values in variables or a state struct.
2. Create LVGL objects once during startup.
3. Update the LVGL objects whenever the variables change.
4. Use `lvgl_port_lock()` whenever you touch LVGL from a task or callback outside the LVGL task.

That separation matters. The screen should not own the data. The data should own the screen.

## A Good Dashboard Data Model

For a car dashboard, a simple state structure is enough:

```cpp
struct DashboardState {
    int speedKph;
    int rpm;
    int fuelPercent;
    int coolantTempC;
    int batteryVolts;
    int gear;
    bool checkEngine;
    bool seatBelt;
    bool turnSignalLeft;
    bool turnSignalRight;
};
```

You can fill this struct from sensors, CAN bus messages, simulated values, or network data. The UI then reads from the struct and renders the latest values.

## Recommended LVGL Layout For A Car Dashboard

For an 800x480 screen, a practical layout is:

- Left side: speedometer or large numeric speed readout.
- Center: RPM gauge or arc.
- Top strip: battery, coolant temperature, gear, time, and connectivity status.
- Bottom strip: warning icons and touch buttons.
- Right side: fuel gauge, trip info, or media controls.

This example already targets a full-screen LVGL app, so the layout can be composed using standard LVGL objects such as `lv_obj`, `lv_label`, `lv_arc`, `lv_bar`, `lv_meter`, and `lv_btn`.

## Minimal Startup Flow

The startup sequence in this folder is already close to what you need:

1. Initialize the board.
2. Initialize LVGL with the LCD and touch pointer.
3. Create your dashboard widgets.
4. Start a periodic update path for your live data.

The important detail is that the dashboard widgets should be created once, not rebuilt every frame.

## Replacing The Demo UI

In [09_lvgl_Porting.ino](09_lvgl_Porting.ino), replace:

```cpp
lv_demo_widgets();
```

with something like:

```cpp
create_dashboard_ui();
```

That function should build all objects and keep their handles in global or static variables so you can update them later.

## Example Dashboard UI Structure

Here is a compact example of the kind of objects you might create:

```cpp
static lv_obj_t *speed_label;
static lv_obj_t *rpm_arc;
static lv_obj_t *fuel_bar;
static lv_obj_t *temp_label;
static lv_obj_t *gear_label;

void create_dashboard_ui()
{
    lv_obj_t *screen = lv_scr_act();

    speed_label = lv_label_create(screen);
    lv_label_set_text(speed_label, "0");
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
    lv_obj_align(speed_label, LV_ALIGN_LEFT_MID, 30, -20);

    rpm_arc = lv_arc_create(screen);
    lv_obj_set_size(rpm_arc, 220, 220);
    lv_obj_align(rpm_arc, LV_ALIGN_CENTER, 0, 0);

    fuel_bar = lv_bar_create(screen);
    lv_obj_set_size(fuel_bar, 220, 20);
    lv_obj_align(fuel_bar, LV_ALIGN_BOTTOM_LEFT, 30, -40);

    temp_label = lv_label_create(screen);
    lv_label_set_text(temp_label, "Temp: -- C");
    lv_obj_align(temp_label, LV_ALIGN_TOP_RIGHT, -30, 20);

    gear_label = lv_label_create(screen);
    lv_label_set_text(gear_label, "P");
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_40, 0);
    lv_obj_align(gear_label, LV_ALIGN_TOP_MID, 0, 20);
}
```

This is only the shape of the UI. A polished dashboard would add color themes, icons, gradients, warning states, and animations.

## Updating The Screen From Device Variables

There are two common ways to feed LVGL from variables:

### 1. Polling a state structure on a timer

This is the easiest model. Use a periodic timer or task to read your variables and push the values into LVGL.

```cpp
void refresh_dashboard_ui(const DashboardState &state)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%d", state.speedKph);
    lv_label_set_text(speed_label, buf);

    lv_arc_set_value(rpm_arc, state.rpm);
    lv_bar_set_value(fuel_bar, state.fuelPercent, LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "Temp: %d C", state.coolantTempC);
    lv_label_set_text(temp_label, buf);

    if (state.gear == 0) {
        lv_label_set_text(gear_label, "P");
    } else if (state.gear == 1) {
        lv_label_set_text(gear_label, "D");
    } else {
        lv_label_set_text_fmt(gear_label, "%d", state.gear);
    }
}
```

### 2. Updating on data arrival

If sensor data arrives from another task, CAN callback, serial parser, or network handler, update the state there and then refresh the widgets.

When you do that outside the LVGL task, wrap the UI update with the port mutex:

```cpp
if (lvgl_port_lock(-1)) {
    refresh_dashboard_ui(currentState);
    lvgl_port_unlock();
}
```

This is important because LVGL is not thread-safe.

## Where The Data Can Come From

The variables driving the dashboard can come from many sources:

- ADC readings for battery voltage or analog sensors.
- I2C or SPI sensor chips.
- UART telemetry.
- CAN bus messages from the vehicle network.
- Bluetooth or Wi-Fi data.
- Simulated values for development.

The dashboard does not care where the values came from. It only cares that the values are updated consistently.

## Touch Interaction

Touch support is already wired into this example through `board->getTouch()` and `lvgl_port_init()`.

For the current custom board configuration, touch is disabled by default because `ESP_OPEN_TOUCH` is `0` in [esp_panel_board_custom_conf.h](esp_panel_board_custom_conf.h). If you want dashboard touch controls, enable it there so the touch device is initialized and passed into LVGL.

Useful touch actions for a car-style UI include:

- Tap to switch between speed, trip, media, and diagnostics views.
- Long press to open settings.
- Swipe to change pages.
- Tap warning icons to open detailed fault screens.

## Using LVGL Objects For A Dashboard

Some LVGL widgets are especially useful for automotive-style layouts:

- `lv_label` for numeric readouts and text status.
- `lv_arc` for gauges such as RPM or temperature.
- `lv_bar` for fuel, battery, or signal strength.
- `lv_meter` for speedometers and richer dial graphics.
- `lv_img` for icons and indicator lights.
- `lv_btn` for touch targets.

If you want a more instrument-cluster look, use larger typography, high-contrast colors, and a dark background.

## Example Update Loop

If your values change on a timer, a simple task can keep the dashboard refreshed:

```cpp
void dashboard_task(void *param)
{
    while (true) {
        DashboardState state = read_dashboard_state();

        if (lvgl_port_lock(-1)) {
            refresh_dashboard_ui(state);
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

This is a clean fit for the LVGL task model used in this example because it keeps screen updates predictable and avoids rebuilding objects.

## Performance Notes For An 800x480 Screen

An 800x480 dashboard can look good and still run smoothly if you keep a few rules in mind:

- Avoid recreating objects every refresh.
- Update only the widgets that actually changed.
- Prefer text updates over full-screen redraws when possible.
- Use animation sparingly for gauges and transitions.
- Keep fonts and images reasonably sized.
- If the UI feels slow, reduce visual complexity before changing the refresh rate.

The LVGL port in this example already handles the display task and mutex management. Your main job is to keep the UI logic efficient.

## Practical Car Dashboard Example

Imagine a live dashboard with these data fields:

- `speedKph` from wheel sensors or vehicle telemetry.
- `rpm` from engine data.
- `fuelPercent` from the fuel sender.
- `coolantTempC` from the engine temperature sensor.
- `gear` from transmission state.
- `checkEngine` from fault codes.

The screen could show:

- A large speed number in the center-left.
- A circular RPM gauge in the center.
- Fuel and temperature bars along the edges.
- Warning icons for seat belt, check engine, and turn signals.
- A touch button row to swap between a normal driving view and a diagnostics view.

That entire UI can be built with standard LVGL widgets and refreshed from the device variables every 50 to 250 ms depending on how dynamic the data is.

## Suggested Implementation Order

1. Define a dashboard state struct.
2. Build a static LVGL screen with labels, arcs, bars, and icons.
3. Connect your sensor or telemetry source to the state struct.
4. Add a refresh function that copies state into widgets.
5. Call the refresh function from a task or timer under `lvgl_port_lock()`.
6. Enable touch input if you want page switching or settings controls.

## Minimal Integration Checklist

- Replace `lv_demo_widgets()` with your own UI creation function.
- Store widget handles globally or in a dashboard struct.
- Update those widgets from your device variables.
- Lock LVGL before calling any LVGL API outside the LVGL task.
- Enable touch in [esp_panel_board_custom_conf.h](esp_panel_board_custom_conf.h) if you need input.

## Summary

This example already gives you the display driver, the touch hook, the LVGL task, and the synchronization primitives. That is enough to build a real data-driven interface. For a car dashboard, the main idea is simple: create the instrument cluster once, then keep pushing live vehicle values into those widgets.

The result is a responsive dashboard that looks like a real product instead of a demo screen.