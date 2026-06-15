#!/usr/bin/env python3
import os
import sys
import tkinter as tk


class PpmLiveViewer:
    def __init__(self, root, path):
        self.root = root
        self.path = path
        self.last_mtime = None
        self.image = None

        self.root.title(path)
        self.label = tk.Label(root, bg="black")
        self.label.pack()
        self.status = tk.Label(root, anchor="w")
        self.status.pack(fill="x")

        self.poll()

    def poll(self):
        try:
            mtime = os.path.getmtime(self.path)
        except OSError:
            self.status.config(text=f"waiting for {self.path}")
            self.root.after(100, self.poll)
            return

        if mtime != self.last_mtime:
            self.last_mtime = mtime
            try:
                self.image = tk.PhotoImage(file=self.path)
                self.label.config(image=self.image)
                self.status.config(text=f"{self.path} updated")
            except tk.TclError as exc:
                self.status.config(text=f"reload failed: {exc}")

        self.root.after(33, self.poll)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "screen.ppm"
    root = tk.Tk()
    PpmLiveViewer(root, path)
    root.mainloop()


if __name__ == "__main__":
    main()
