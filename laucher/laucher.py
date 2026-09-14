import customtkinter as ctk
import serial
import serial.tools.list_ports
import requests
import json
import os
import threading
import subprocess
import sys
import shutil
import time
from tkinter import filedialog

# ============================================================
# REX LAUNCHER
# ============================================================

ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("dark-blue")

BLACK = "#0d0d0d"
PANEL = "#161616"
PANEL2 = "#1d1d1d"
ORANGE = "#ff5500"
ORANGE_HOVER = "#cc4400"
WHITE = "#f2f2f2"
GRAY = "#888888"
GREEN = "#35c759"
RED = "#ff453a"

URL_FILE = "rex_urls.json"
BAUD_RATE = 115200
HANDSHAKE_TIMEOUT = 15
DEVICE_READY_RESPONSE = "REX_READY"


# ============================================================
# URL STORAGE
# ============================================================

def load_urls():
    if not os.path.exists(URL_FILE):
        return []

    try:
        with open(URL_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return []


def save_urls(urls):
    with open(URL_FILE, "w", encoding="utf-8") as f:
        json.dump(urls, f, indent=4)


# ============================================================
# MAIN APP
# ============================================================

class RexLauncher(ctk.CTk):

    def __init__(self):
        super().__init__()

        self.title("REX Launcher")
        self.geometry("1050x680")
        self.minsize(900, 600)

        self.configure(fg_color=BLACK)

        self.serial = None
        self.rex_app_process = None
        self.closing = False
        self.urls = load_urls()

        self.protocol("WM_DELETE_WINDOW", self.on_close)

        self.build_ui()

        self.refresh_ports()

    # ========================================================
    # UI
    # ========================================================

    def build_ui(self):

        # ---------- HEADER ----------

        self.header = ctk.CTkFrame(
            self,
            height=70,
            fg_color=PANEL,
            corner_radius=0
        )
        self.header.pack(fill="x")
        self.header.pack_propagate(False)

        ctk.CTkLabel(
            self.header,
            text="🦖 REX",
            font=("Consolas", 26, "bold"),
            text_color=ORANGE
        ).pack(side="left", padx=25)

        ctk.CTkLabel(
            self.header,
            text="CONTROL CENTER",
            font=("Consolas", 13),
            text_color=GRAY
        ).pack(side="left")

        self.connection_label = ctk.CTkLabel(
            self.header,
            text="● DISCONNECTED",
            font=("Consolas", 13, "bold"),
            text_color=RED
        )
        self.connection_label.pack(side="right", padx=25)

        # ---------- BODY ----------

        body = ctk.CTkFrame(
            self,
            fg_color=BLACK,
            corner_radius=0
        )
        body.pack(fill="both", expand=True)

        # ---------- SIDEBAR ----------

        sidebar = ctk.CTkFrame(
            body,
            width=210,
            fg_color=PANEL,
            corner_radius=0
        )
        sidebar.pack(side="left", fill="y")
        sidebar.pack_propagate(False)

        ctk.CTkLabel(
            sidebar,
            text="REX",
            font=("Consolas", 14, "bold"),
            text_color=GRAY
        ).pack(pady=(30, 15))

        self.add_nav_button(sidebar, "Dashboard", self.show_dashboard)
        self.add_nav_button(sidebar, "Device", self.show_device)
        self.add_nav_button(sidebar, "URL Manager", self.show_urls)
        self.add_nav_button(sidebar, "Controls", self.show_controls)
        self.add_nav_button(sidebar, "Services", self.show_services)
        self.add_nav_button(sidebar, "Scripts", self.show_scripts)

        # ---------- CONTENT ----------

        self.content = ctk.CTkFrame(
            body,
            fg_color=BLACK,
            corner_radius=0
        )
        self.content.pack(
            side="left",
            fill="both",
            expand=True
        )

        self.show_dashboard()

    # ========================================================
    # NAVIGATION
    # ========================================================

    def add_nav_button(self, parent, text, command):

        btn = ctk.CTkButton(
            parent,
            text=text,
            height=42,
            anchor="w",
            font=("Consolas", 14),
            fg_color="transparent",
            hover_color="#292929",
            command=command
        )

        btn.pack(
            fill="x",
            padx=12,
            pady=3
        )

    def clear_content(self):

        for widget in self.content.winfo_children():
            widget.destroy()

    # ========================================================
    # DASHBOARD
    # ========================================================

    def show_dashboard(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="Dashboard",
            font=("Consolas", 30, "bold"),
            text_color=WHITE
        ).pack(
            anchor="w",
            padx=35,
            pady=(35, 5)
        )

        ctk.CTkLabel(
            self.content,
            text="Rex developer cyberdeck control center",
            font=("Consolas", 14),
            text_color=GRAY
        ).pack(
            anchor="w",
            padx=37
        )

        cards = ctk.CTkFrame(
            self.content,
            fg_color="transparent"
        )
        cards.pack(
            fill="x",
            padx=35,
            pady=35
        )

        self.status_card(
            cards,
            "DEVICE",
            "CONNECTED" if self.is_serial_connected() else "OFFLINE",
            GREEN if self.is_serial_connected() else RED
        )

        self.status_card(
            cards,
            "SAVED URLS",
            str(len(self.urls)),
            ORANGE
        )

        self.status_card(
            cards,
            "BAUD",
            str(BAUD_RATE),
            WHITE
        )

        # quick controls

        quick = ctk.CTkFrame(
            self.content,
            fg_color=PANEL,
            corner_radius=12
        )
        quick.pack(
            fill="x",
            padx=35,
            pady=10
        )

        ctk.CTkLabel(
            quick,
            text="QUICK CONTROL",
            font=("Consolas", 15, "bold"),
            text_color=ORANGE
        ).pack(
            anchor="w",
            padx=20,
            pady=(18, 12)
        )

        row = ctk.CTkFrame(
            quick,
            fg_color="transparent"
        )
        row.pack(
            fill="x",
            padx=20,
            pady=(0, 20)
        )

        self.control_button(row, "◀ PREV", "p")
        self.control_button(row, "MAIN", "m")
        self.control_button(row, "NEXT ▶", "n")
        self.control_button(row, "POWER", "o")

    def status_card(self, parent, title, value, color):

        card = ctk.CTkFrame(
            parent,
            width=190,
            height=100,
            fg_color=PANEL,
            corner_radius=12
        )

        card.pack(
            side="left",
            padx=(0, 15)
        )

        card.pack_propagate(False)

        ctk.CTkLabel(
            card,
            text=title,
            font=("Consolas", 11),
            text_color=GRAY
        ).pack(
            anchor="w",
            padx=15,
            pady=(15, 3)
        )

        ctk.CTkLabel(
            card,
            text=value,
            font=("Consolas", 18, "bold"),
            text_color=color
        ).pack(
            anchor="w",
            padx=15
        )

    # ========================================================
    # SERIAL
    # ========================================================

    def is_serial_connected(self):
        return self.serial is not None and self.serial.is_open

    def refresh_ports(self):

        ports = [
            port.device
            for port in serial.tools.list_ports.comports()
        ]

        if hasattr(self, "port_menu"):

            self.port_menu.configure(
                values=ports if ports else ["No ports"]
            )

            if ports:
                self.port_menu.set(ports[0])

    def connect_serial(self):

        port = self.port_menu.get()

        if not port or port == "No ports":
            return

        try:

            connection = serial.Serial(
                port,
                BAUD_RATE,
                timeout=0.2
            )

            self.connection_label.configure(
                text="● VERIFYING",
                text_color=ORANGE
            )
            connection.reset_input_buffer()
            connection.write(b"ping\n")
            deadline = time.monotonic() + HANDSHAKE_TIMEOUT
            device_ready = False
            while time.monotonic() < deadline:
                response = connection.readline().decode(
                    "utf-8",
                    errors="ignore"
                ).strip()
                if response == DEVICE_READY_RESPONSE:
                    device_ready = True
                    break

            if not device_ready:
                connection.close()
                self.serial = None
                self.connection_label.configure(
                    text="● NOT REX-SA",
                    text_color=RED
                )
                return

            self.serial = connection
            self.connection_label.configure(
                text="● CONNECTED",
                text_color=GREEN
            )

            self.refresh_ports()

        except Exception as e:

            self.serial = None

            self.connection_label.configure(
                text="● ERROR",
                text_color=RED
            )

            print("Serial error:", e)

    def disconnect_serial(self):

        if self.serial:

            try:
                self.serial.close()
            except:
                pass

        self.serial = None

        self.connection_label.configure(
            text="● DISCONNECTED",
            text_color=RED
        )

    def send_serial(self, text):

        if not self.is_serial_connected():
            return

        try:

            self.serial.write(
                (text + "\n").encode("utf-8")
            )

        except Exception as e:

            print("Send error:", e)

    # ========================================================
    # DEVICE PAGE
    # ========================================================

    def show_device(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="Device",
            font=("Consolas", 30, "bold")
        ).pack(
            anchor="w",
            padx=35,
            pady=(35, 25)
        )

        frame = ctk.CTkFrame(
            self.content,
            fg_color=PANEL,
            corner_radius=12
        )
        frame.pack(
            fill="x",
            padx=35
        )

        ctk.CTkLabel(
            frame,
            text="SERIAL PORT",
            text_color=GRAY,
            font=("Consolas", 11)
        ).pack(
            anchor="w",
            padx=20,
            pady=(20, 5)
        )

        row = ctk.CTkFrame(
            frame,
            fg_color="transparent"
        )
        row.pack(
            fill="x",
            padx=20,
            pady=(0, 20)
        )

        ports = [
            port.device
            for port in serial.tools.list_ports.comports()
        ]

        self.port_menu = ctk.CTkComboBox(
            row,
            values=ports if ports else ["No ports"],
            width=220,
            font=("Consolas", 13)
        )
        self.port_menu.pack(side="left")

        ctk.CTkButton(
            row,
            text="REFRESH",
            width=100,
            command=self.refresh_ports
        ).pack(side="left", padx=10)

        ctk.CTkButton(
            row,
            text="CONNECT",
            width=100,
            fg_color=ORANGE,
            hover_color=ORANGE_HOVER,
            command=self.connect_serial
        ).pack(side="left", padx=5)

        ctk.CTkButton(
            row,
            text="DISCONNECT",
            width=110,
            command=self.disconnect_serial
        ).pack(side="left", padx=5)

    # ========================================================
    # CONTROLS
    # ========================================================

    def show_controls(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="Rex-SA Controls",
            font=("Consolas", 30, "bold")
        ).pack(
            anchor="w",
            padx=35,
            pady=(35, 10)
        )

        ctk.CTkLabel(
            self.content,
            text="USB serial control interface",
            text_color=GRAY,
            font=("Consolas", 13)
        ).pack(
            anchor="w",
            padx=37
        )

        panel = ctk.CTkFrame(
            self.content,
            fg_color=PANEL,
            corner_radius=12
        )
        panel.pack(
            padx=35,
            pady=35,
            fill="x"
        )

        row = ctk.CTkFrame(
            panel,
            fg_color="transparent"
        )
        row.pack(
            pady=35
        )

        self.control_button(row, "◀ PREV", "p")
        self.control_button(row, "MAIN", "m")
        self.control_button(row, "NEXT ▶", "n")
        self.control_button(row, "POWER", "o")

    def control_button(self, parent, text, command):

        ctk.CTkButton(
            parent,
            text=text,
            width=130,
            height=55,
            font=("Consolas", 15, "bold"),
            fg_color=ORANGE,
            hover_color=ORANGE_HOVER,
            command=lambda: self.send_serial(command)
        ).pack(
            side="left",
            padx=7
        )

    # ========================================================
    # URL MANAGER
    # ========================================================

    def show_urls(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="URL Manager",
            font=("Consolas", 30, "bold")
        ).pack(
            anchor="w",
            padx=35,
            pady=(35, 5)
        )

        ctk.CTkLabel(
            self.content,
            text="Services Rex should remember and monitor",
            font=("Consolas", 13),
            text_color=GRAY
        ).pack(
            anchor="w",
            padx=37
        )

        toolbar = ctk.CTkFrame(
            self.content,
            fg_color="transparent"
        )
        toolbar.pack(
            fill="x",
            padx=35,
            pady=25
        )

        ctk.CTkButton(
            toolbar,
            text="+ ADD URL",
            fg_color=ORANGE,
            hover_color=ORANGE_HOVER,
            command=self.add_url_dialog
        ).pack(side="left")

        ctk.CTkButton(
            toolbar,
            text="CHECK ALL",
            command=self.check_all_urls
        ).pack(side="left", padx=10)

        self.url_list = ctk.CTkScrollableFrame(
            self.content,
            fg_color=PANEL,
            corner_radius=12
        )
        self.url_list.pack(
            fill="both",
            expand=True,
            padx=35,
            pady=(0, 30)
        )

        self.render_urls()

    def render_urls(self):

        for widget in self.url_list.winfo_children():
            widget.destroy()

        for index, item in enumerate(self.urls):

            row = ctk.CTkFrame(
                self.url_list,
                fg_color=PANEL2,
                corner_radius=8
            )
            row.pack(
                fill="x",
                padx=8,
                pady=6
            )

            info = ctk.CTkFrame(
                row,
                fg_color="transparent"
            )
            info.pack(
                side="left",
                fill="x",
                expand=True,
                padx=15,
                pady=12
            )

            ctk.CTkLabel(
                info,
                text=item["name"],
                font=("Consolas", 15, "bold"),
                text_color=WHITE
            ).pack(anchor="w")

            ctk.CTkLabel(
                info,
                text=item["url"],
                font=("Consolas", 11),
                text_color=GRAY
            ).pack(anchor="w", pady=(3, 0))

            ctk.CTkButton(
                row,
                text="SEND",
                width=70,
                command=lambda u=item["url"]: self.send_url(u)
            ).pack(side="right", padx=5)

            ctk.CTkButton(
                row,
                text="CHECK",
                width=70,
                command=lambda i=index: self.check_url(i)
            ).pack(side="right", padx=5)

            ctk.CTkButton(
                row,
                text="DELETE",
                width=70,
                fg_color="#333333",
                hover_color="#444444",
                command=lambda i=index: self.delete_url(i)
            ).pack(side="right", padx=5)

    # ========================================================
    # ADD URL
    # ========================================================

    def add_url_dialog(self):

        dialog = ctk.CTkToplevel(self)

        dialog.title("Add URL")
        dialog.geometry("420x280")
        dialog.configure(fg_color=PANEL)

        dialog.transient(self)
        dialog.grab_set()

        ctk.CTkLabel(
            dialog,
            text="ADD SERVICE",
            font=("Consolas", 20, "bold"),
            text_color=ORANGE
        ).pack(pady=(25, 20))

        name_entry = ctk.CTkEntry(
            dialog,
            placeholder_text="Service name"
        )
        name_entry.pack(
            fill="x",
            padx=30,
            pady=8
        )

        url_entry = ctk.CTkEntry(
            dialog,
            placeholder_text="https://example.com"
        )
        url_entry.pack(
            fill="x",
            padx=30,
            pady=8
        )

        def save():

            name = name_entry.get().strip()
            url = url_entry.get().strip()

            if not name or not url:
                return

            self.urls.append({
                "name": name,
                "url": url,
                "status": "UNKNOWN"
            })

            save_urls(self.urls)

            dialog.destroy()

            self.show_urls()

        ctk.CTkButton(
            dialog,
            text="SAVE",
            fg_color=ORANGE,
            hover_color=ORANGE_HOVER,
            command=save
        ).pack(
            pady=20
        )

    # ========================================================
    # DELETE URL
    # ========================================================

    def delete_url(self, index):
        if 0 <= index < len(self.urls):
            url = self.urls[index]["url"]

            # Tell Rex-SA to remove it
            # Firmware expects: urldel>https://example.com
            self.send_serial("urldel>" + url)

            del self.urls[index]
            save_urls(self.urls)
            self.render_urls()

    # ========================================================
    # SEND URL
    # ========================================================

    def send_url(self, url):
        # Firmware expects: url>https://example.com
        self.send_serial("url>" + url)

    # ========================================================
    # CHECK URL
    # ========================================================

    def check_url(self, index):

        if index >= len(self.urls):
            return

        url = self.urls[index]["url"]

        def worker():

            try:

                response = requests.get(
                    url,
                    timeout=5
                )

                if response.status_code < 400:
                    status = "LIVE"
                else:
                    status = f"HTTP {response.status_code}"

            except Exception:
                status = "DOWN"

            self.urls[index]["status"] = status
            save_urls(self.urls)

            self.after(
                0,
                self.render_urls
            )

        threading.Thread(
            target=worker,
            daemon=True
        ).start()

    def check_all_urls(self):

        for i in range(len(self.urls)):
            self.check_url(i)

    # ========================================================
    # SERVICES
    # ========================================================

    def show_services(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="Services",
            font=("Consolas", 30, "bold")
        ).pack(
            anchor="w",
            padx=35,
            pady=(35, 10)
        )

        ctk.CTkLabel(
            self.content,
            text="Saved Rex services",
            font=("Consolas", 13),
            text_color=GRAY
        ).pack(
            anchor="w",
            padx=37
        )

        for item in self.urls:

            frame = ctk.CTkFrame(
                self.content,
                fg_color=PANEL,
                corner_radius=10
            )

            frame.pack(
                fill="x",
                padx=35,
                pady=7
            )

            ctk.CTkLabel(
                frame,
                text=item["name"],
                font=("Consolas", 15, "bold")
            ).pack(
                side="left",
                padx=20,
                pady=15
            )

            ctk.CTkLabel(
                frame,
                text=item.get("status", "UNKNOWN"),
                font=("Consolas", 12),
                text_color=(
                    GREEN
                    if item.get("status") == "LIVE"
                    else RED
                    if item.get("status") == "DOWN"
                    else GRAY
                )
            ).pack(
                side="right",
                padx=20
            )
    # ========================================================
    # SCRIPTS (run main.py only if Rex-SA is connected)
    # ========================================================

    def show_scripts(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content,
            text="Scripts",
            font=("Consolas", 30, "bold"),
            text_color=WHITE
        ).pack(anchor="w", padx=35, pady=(35, 5))

        ctk.CTkLabel(
            self.content,
            text="Runs rex-app main.py when Rex-SA is connected",
            font=("Consolas", 13),
            text_color=GRAY
        ).pack(anchor="w", padx=37)

        panel = ctk.CTkFrame(
            self.content,
            fg_color=PANEL,
            corner_radius=12
        )
        panel.pack(fill="x", padx=35, pady=30)

        btn_row = ctk.CTkFrame(panel, fg_color="transparent")
        btn_row.pack(fill="x", padx=20, pady=20)

        ctk.CTkButton(
            btn_row,
            text="RUN REX-APP",
            width=160,
            height=40,
            fg_color=ORANGE,
            hover_color=ORANGE_HOVER,
            font=("Consolas", 14, "bold"),
            command=self.run_script
        ).pack(side="left")

        self.script_status = ctk.CTkLabel(
            btn_row,
            text="Waiting…",
            font=("Consolas", 12),
            text_color=GRAY
        )
        self.script_status.pack(side="left", padx=15)

        ctk.CTkLabel(
            self.content,
            text="OUTPUT",
            font=("Consolas", 12, "bold"),
            text_color=GRAY
        ).pack(anchor="w", padx=37, pady=(5, 5))

        self.script_output = ctk.CTkTextbox(
            self.content,
            font=("Consolas", 12),
            fg_color=PANEL,
            text_color=WHITE,
            corner_radius=10
        )
        self.script_output.pack(
            fill="both",
            expand=True,
            padx=35,
            pady=(0, 25)
        )

    def run_script(self):

        if not self.is_serial_connected():
            self.script_status.configure(
                text="Rex-SA not connected",
                text_color=RED
            )
            return

        # ========== SET THESE ==========
        app_dir = r"C:\Users\User\OneDrive\projects\rex-app\app"
        path = os.path.join(app_dir, "main.py")
        # ===============================

        if not os.path.isfile(path):
            self.script_status.configure(
                text="main.py not found",
                text_color=RED
            )
            return

        # In a PyInstaller build, sys.executable is rex_launcher.exe itself.
        # Use the installed Python interpreter when launching the external app.
        if getattr(sys, "frozen", False):
            python_executable = shutil.which("pythonw")
            if python_executable:
                cmd = [python_executable, path]
            else:
                python_executable = shutil.which("python")
                if python_executable:
                    cmd = [python_executable, path]
                else:
                    self.script_status.configure(
                        text="Python not found",
                        text_color=RED
                    )
                    self.script_output.delete("1.0", "end")
                    self.script_output.insert(
                        "end",
                        "Install Python or add it to PATH before running rex-app.\n"
                    )
                    return
        else:
            cmd = [sys.executable, path]

        self.script_status.configure(text="Running…", text_color=ORANGE)
        self.script_output.delete("1.0", "end")
        self.script_output.insert("end", f"$ cd {app_dir}\n$ {' '.join(cmd)}\n\n")

        def worker():
            process = None
            timed_out = False
            try:
                process = subprocess.Popen(
                    cmd,
                    cwd=app_dir,          # important for: from ui.features import ...
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=subprocess.CREATE_NO_WINDOW
                )
                self.rex_app_process = process
                if self.closing:
                    process.terminate()

                try:
                    out, err = process.communicate(timeout=120)
                    code = process.returncode
                except subprocess.TimeoutExpired:
                    timed_out = True
                    process.kill()
                    out, err = process.communicate()
                    code = process.returncode

                def done():
                    if self.closing:
                        return
                    if out:
                        self.script_output.insert("end", out)
                    if err:
                        self.script_output.insert("end", err)
                    self.script_output.insert("end", f"\n[exit {code}]\n")
                    if timed_out:
                        self.script_status.configure(text="Timed out", text_color=RED)
                        self.script_output.insert("end", "Script timed out (120s).\n")
                    elif code == 0:
                        self.script_status.configure(text="Done", text_color=GREEN)
                    else:
                        self.script_status.configure(text=f"Failed ({code})", text_color=RED)

                self.after(0, done)
            except Exception as e:
                if not self.closing:
                    error_message = str(e)
                    self.after(0, lambda: (
                        self.script_status.configure(text="Error", text_color=RED),
                        self.script_output.insert("end", error_message + "\n")
                    ))
            finally:
                if self.rex_app_process is process:
                    self.rex_app_process = None

        threading.Thread(target=worker, daemon=True).start()

    # ========================================================
    # CLOSE
    # ========================================================

    def on_close(self):

        self.closing = True
        if self.rex_app_process and self.rex_app_process.poll() is None:
            self.rex_app_process.terminate()
        self.disconnect_serial()
        self.destroy()


# ============================================================
# RUN
# ============================================================

if __name__ == "__main__":

    app = RexLauncher()
    app.mainloop()