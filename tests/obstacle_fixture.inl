static void obstacle_policy_tests() {
    ObstacleHistory h={};
    for(unsigned i=0;i<11;++i) {
        obstacle_sample(h,100+static_cast<int>(i)*10,101+i%2,200,99-static_cast<int>(i%2),100);
        check(obstacle_oscillating(h)==(i==10),"oscillation requires ten complete game turns");
    }
    unsigned count=h.count;obstacle_sample(h,200,102,200,98,100);
    check(h.count==count && h.hex[10]==101,"repeated AI callbacks in one turn cannot accelerate history");
    obstacle_sample(h,210,101,201,100,100);check(h.count==1,"new objective resets old movement history");
    obstacle_sample(h,220,102,201,99,220);check(h.count==1,"actual capture or construction progress resets history");
    obstacle_sample(h,230,101,201,100,220);obstacle_sample(h,210,102,201,99,220);
    check(h.count==1,"loading an earlier date resets motion history");
    obstacle_sample(h,240,101,201,100,220);check(h.count==1,"unobserved intervening turns cannot be guessed as oscillation");
    for(unsigned pattern=0;pattern<5;++pattern) {
        h={};
        for(unsigned i=0;i<11;++i) {
            unsigned hex=pattern==0?101:pattern==1?101+i:pattern==2?120-i:pattern==3?101+i%2:101+i;
            int range=pattern==0?99:pattern==1?99-static_cast<int>(i):pattern==2?80+static_cast<int>(i):pattern==3?99-static_cast<int>(i%2):99-static_cast<int>(i%2);
            obstacle_sample(h,100+static_cast<int>(i)*10,hex,200,range,100);
        }
        check(obstacle_oscillating(h)==(pattern==3),"stationary, normal advance, withdrawal and non-repeating detours do not trigger demolition");
    }
}
