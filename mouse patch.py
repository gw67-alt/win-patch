from pynput import mouse
import time
import logging

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')

class MouseMonitor:
    def __init__(self, threshold=100):
        self.threshold = threshold
        self.previous_position = None
        self.listener = mouse.Listener(on_move=self.on_move)
        self.listener.start()

    def on_move(self, x, y):
        if self.previous_position:
            prev_x, prev_y = self.previous_position
            if abs(x - prev_x) > self.threshold or abs(y - prev_y) > self.threshold:
                logging.info(f"Jump detected! Reverting to previous position: ({prev_x}, {prev_y})")
                self.revert_position()
        self.previous_position = (x, y)

    def revert_position(self):
        if self.previous_position:
            mouse.Controller().position = self.previous_position
            logging.info(f"Position reverted to: {self.previous_position}")

if __name__ == "__main__":
    monitor = MouseMonitor(threshold=50)  # Set the threshold for detecting jumps
    logging.info("Mouse monitor started. Press Ctrl+C to exit.")
    try:
        while True:
            time.sleep(1)  # Keep the program running
    except KeyboardInterrupt:
        logging.info("Mouse monitor stopped.")