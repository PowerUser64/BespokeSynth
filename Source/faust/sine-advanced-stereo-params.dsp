import("stdfaust.lib");

f = hslider("freq", 220, 55, 880, 0.01);

// multiply the output by the input, then scale by 1/2
process = ((_ + 1) * os.osc(f)) * 0.5,
          ((_ + 1) * os.osc(f)) * 0.5;