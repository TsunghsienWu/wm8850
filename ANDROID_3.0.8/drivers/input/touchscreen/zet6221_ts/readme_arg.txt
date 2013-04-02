Abount wmt.io.touch:
format:
enable:ID:irq_gpio:x_resolution:y_resolution:reset_gpio:x-axis:x_direction:y_direction:max_finger_num

item defination:
	enable:1--load the driver,0--doesn't load the driver;
	irq_gpio: the interrupt gpio.Now it is 9;
	ID:one string to indicate the touch IC or TP;
	x_resolution: the resolution of x-axis;
	y_resolution: the resolution of y-axis;
	reset_gpio: the reset gpio.Now it is 4.Note:For ft5406 and ft5204 of 7",the gpio should be cut by the hardware;
	x-axis:0--x coordinate,1--x coordinate swaps with y coordinate;
	x_direction:-1--indicate x reverts,1--x coordinate doesn't revert;
	y_direction:-1--indicate y reverts,1--y coordinate doesn't revert;
	max_finger_num: The default is 5;

wmt.io.tskeyindex num:key1:key2:key3

// quicktest param
wmt.quicktest.touch resolutionX:resolutionY	

// pufangda levo
setenv wmt.io.touch 1:ZET6221_7why1205:9:480:800:4:1:-1:1:5

wmtenv set wmt.io.touch 1:ZET6221_7why1205:9:640:940:4:1:-1:1:5

// pufangda levo DGNTPC0350
960*640

setenv wmt.io.touch 1:ZET6221_7dgntpc0350:9:640:960:4:1:-1:1:5

wmtenv set wmt.io.touch 1:ZET6221_7dgntpc0350:9:640:960:4:1:-1:1:5

// pufangda levo zcc1950
setenv wmt.io.touch 1:ZET6221_7zcc1950:9:640:960:4:1:1:-1:5

wmtenv set wmt.io.touch 1:ZET6221_7zcc1950:9:640:960:4:1:1:-1:5
	
// sunday
// 3 key tpc0312
setenv wmt.io.touch 1:ZET6221_7tpc0312:9:480:800:4:1:-1:1:5

wmtenv set wmt.io.touch 1:ZET6221_7tpc0312:9:480:800:4:1:-1:1:5

// no key tpc0311
setenv wmt.io.touch 1:ZET6221_7tpc0311:9:480:800:4:1:-1:1:5

// DGNTPC0406 levo
setenv wmt.io.touch 1:ZET6221_8dgntpc0406:9:600:800:4:1:-1:-1:5

// XDC806
wmtenv set wmt.io.touch 1:ZET6221_8xdc806:9:600:800:4:1:-1:-1:5


// ADC700148 Xindi    ZET6221_7adc700148_fw
setenv wmt.io.touch 1:ZET6221_7adc700148:9:480:800:4:1:-1:1:5
setenv wmt.io.tskeyindex 3:158:139:102

// TP070005Q8  ZET6221_7tp070005q8_fw
setenv wmt.io.touch 1:ZET6221_7tp070005q8:9:480:800:4:1:-1:1:5

// Yiheng7002   ZET6221_7yiheng7002_fw
setenv wmt.io.touch 1:ZET6221_7yiheng7002:9:480:800:4:1:-1:1:5

// ATC7031 (levo)  ZET6221_7atc7031_fw
setenv wmt.io.touch 1:ZET6221_7atc7031:9:480:800:4:1:-1:1:5

// XCLG7027A (lev0) ZET6221_7xclg7027a_fw
setenv wmt.io.touch 1:ZET6221_7xclg7027a:9:640:960:4:1:1:-1:5

//EST07000416   MID7_8271_ZET6221_7est07000416
ZET6221_7est07000416_fw
setenv wmt.io.touch 1:ZET6221_7est07000416:9:480:800:4:1:-1:1:5

//TPT070001   levo  ZET6221_7tpt070001_fw

setenv wmt.io.touch 1:ZET6221_7tpt070001:9:640:960:4:1:1:-1:5

//PW07T010-C xindi ZET6221_7pw07t010c_fw
setenv wmt.io.touch 1:ZET6221_7pw07t010c:9:480:800:4:1:-1:1:5

// CZY6111FPC sunday 
setenv wmt.io.touch 1:ZET6221_7czy6111fpc:9:600:800:4:1:1:-1:5

wmtenv set wmt.io.touch 1:ZET6221_7czy6111fpc:9:600:800:4:1:1:-1:5

// TPT070121 xindi
wmtenv set wmt.io.touch 1:ZET6221_7tpt070121:9:480:800:4:1:1:-1:5

// MF180070F8
wmtenv set wmt.io.touch 1:ZET6221_7mf180070f8:9:480:800:4:1:-1:1:5




