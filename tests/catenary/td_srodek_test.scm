node 1000 0 m1 suspension scheme 1124 1 at 0.687 5.800 100.247 rope 0.687 7.500 100.247 pole 1611-11 -2.313 0.200 100.247 endsuspension
node 1000 0 m2 suspension scheme 1124 1 at 0.687 5.800 160.247 rope 0.687 7.500 160.247 pole 1611-11 -2.313 0.200 160.247 endsuspension
node 1000 0 m3 suspension scheme 1124 1 at 0.687 5.800 220.247 rope 0.687 7.500 220.247 pole 1611-11 -2.313 0.200 220.247 endsuspension
node 1000 0 km anchor device 0461 network C120-2C pole 1611-11 -2.400 0.200 244.000 toward 0.687 5.800 220.247 distance 700.0 endanchor
node -1 0 lm chain power pwr01 voltage 3000 current 4500 resistivity 0.01 material cu thickness 3 state 0 type C120-2C hangers 0.4 4 3 0.04 anchor_end km middle m2 suspensions m1 m2 m3 endchain
