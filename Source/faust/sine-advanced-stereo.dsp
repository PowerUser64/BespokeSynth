import("stdfaust.lib");

// multiply the output by the input, then scale by 1/2
process = ((_ + 1) * os.osc(220)) * 0.5,
          ((_ + 1) * os.osc(220)) * 0.5;