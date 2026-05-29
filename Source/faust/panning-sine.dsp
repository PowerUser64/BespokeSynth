import("stdfaust.lib");

lfo = os.osc(2);
lfo2 = cos(os.sawtooth(1)/2+1);

sig = os.osc(220);

process = sig * lfo2, sig * lfo;