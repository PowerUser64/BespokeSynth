import("stdfaust.lib");

b = button("button");
c = checkbox("checkbox");
s = hslider("hslider", 1, 0, 1, 0.1);
S = vslider("vslider", 1, 0, 1, 0.1);
n = nentry("numentry", 1, 0, 1, 0.1);
g = hbargraph("hbargraph", -1, 1);
G = vbargraph("vbargraph", -1, 1);

// TODO: widget groups
// hgroup
// vgroup
// tgroup??

all = b*c*s*S*n : g : G;

process = _ * all, _ * all;