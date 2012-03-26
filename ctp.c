/*
 * This is a userspace touchscreen driver for cypress ctma300 as used
 * in HP Veer configured for WebOS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * The code was written from scratch, and understanding the
 * device output by xndchn @ gmail
 * It gets lots of help from CM project in 
 * https://github.com/CyanogenMod/android_device_hp_tenderloin.git
 * and cpoy most codes of calc_point()
 */

#include <stdio.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

int spi_fd = 0;
int int_fd = 0;
int ss_fd = 0;
int ctp_fd = 0;

void enable_ctp()
{
	int vdd_fd, xres_fd;
	int rc;
	
	__u8 wordbits = 8;
	__u32 speedhz = 1600000;
	
	if (!spi_fd)
		spi_fd = open("/dev/spidev0.2", O_RDWR);
	if (spi_fd <= 0)
		printf("TScontrol: Cannot open spi - %d\n", errno);

	if (!ss_fd)
		ss_fd = open("/sys/user_hw/pins/ctp/ss/level", O_RDWR);
	if (ss_fd <= 0)
		printf("TScontrol: Cannot open ss - %d\n", errno);
		
	if (!int_fd)
		int_fd = open("/sys/user_hw/pins/ctp/int/irqrequest", O_RDWR);
	if (int_fd <= 0)
		printf("TScontrol: Cannot open int - %d\n", errno);
		
	if (!ctp_fd)
		ctp_fd = open("/sys/user_hw/pins/ctp/int/irq", O_RDONLY);
	if (ctp_fd <= 0)
		printf("TScontrol: Cannot open ctp - %d\n", errno);
		
	usleep(500);
	rc = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &wordbits);
	usleep(500);
	rc = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speedhz);
	usleep(50000);
	
	
	vdd_fd = open("/sys/devices/platform/cy8ctma300/vcpin", O_WRONLY);
	if (vdd_fd < 0)
		printf("TScontrol: Cannot open vdd - %d\n", errno);
	xres_fd = open("/sys/devices/platform/cy8ctma300/xres", O_WRONLY);
	if (xres_fd < 0)
		printf("TScontrol: Cannot open xres - %d\n", errno);
		
	rc = write(xres_fd, "1", 1);
	if (rc != 1)
		printf("TSpower, failed set xres\n");
	usleep(500);
	rc = write(xres_fd, "0", 1);
	if (rc != 1)
		printf("TSpower, failed reset xres\n");
	
	usleep(50000);
	
	rc = write(int_fd, "1", 1);
	if (rc != 1)
		printf("TSpower, failed to enable int\n");
    rc = write(vdd_fd, "1", 1);
	if (rc != 1)
		printf("TSpower, failed to enable vdd\n");
	rc = write(ss_fd, "1", 1);
	if (rc != 1)
		printf("TSpower, failed to enable ss\n");
	
	usleep(500);
		
	rc = write(xres_fd, "1", 1);
	if (rc != 1)
		printf("TSpower, failed set xres\n");
	usleep(500);
	rc = write(xres_fd, "0", 1);
	if (rc != 1)
		printf("TSpower, failed reset xres\n");	
	rc = write(ss_fd, "0", 1);
	if (rc != 1)
		printf("TSpower, failed to reset ss\n");
}
unsigned char init1[] = {0x01, 0x01, 0x61};
unsigned char init2[] = {0x06, 0x0a, 0x61};
unsigned char init3[] = {0x07, 0x07, 0x61};
unsigned char init4[] = {0x10, 0x0b, 0x61};
unsigned char init5[] = {0x20, 0x06, 0x61};
unsigned char init6[] = {0x03, 0x00, 0x62};
unsigned char init7[] = {0x01, 0x40, 0x51};
unsigned char init8[] = {0x01, 0x20, 0x02};

unsigned char *init_array[] = {init1, init2, init3, init4, init5, init6, init7, init8};

void init_ctp()
{
	int rc;
	int i;
	char buf[32];
	
	for (i = 0; i < 8; i++) {
		rc = write(spi_fd, init_array[i], 3);
		if (rc != 3)
			printf("TSpower, failed to write init%d\n", i + 1);
		rc = write(ss_fd, "1", 1);
		if (i < 7)
			rc = write(ss_fd, "0", 1);
	
		usleep(500);
	}
	read(ctp_fd, buf, 2);
}

unsigned char rx_buf[132];
#include <math.h>
#define MAX_TOUCH 3 // Max touches that will be reported
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define X_AXIS_POINTS  7
#define Y_AXIS_POINTS  10
#define X_AXIS_MINUS1 X_AXIS_POINTS - 1 // 6
#define Y_AXIS_MINUS1 Y_AXIS_POINTS - 1 // 9

#define X_RESOLUTION  320
#define Y_RESOLUTION  500
#define X_LOCATION_VALUE ((float)X_RESOLUTION) / ((float)X_AXIS_MINUS1)
#define Y_LOCATION_VALUE ((float)Y_RESOLUTION) / ((float)Y_AXIS_MINUS1)
#define X_RESOLUTION_MINUS1 X_RESOLUTION - 1
#define Y_RESOLUTION_MINUS1 Y_RESOLUTION - 1

#define X_SCREEN 320
#define Y_SCREEN 420

#define TOUCH_INITIAL_THRESHOLD 32
int touch_initial_thresh = TOUCH_INITIAL_THRESHOLD;
// Previous touches that have already been reported will continue to be
// reported so long as they stay above this threshold
#define TOUCH_CONTINUE_THRESHOLD 26
int touch_continue_thresh = TOUCH_CONTINUE_THRESHOLD;
#define LARGE_AREA_UNPRESS 22 //TOUCH_CONTINUE_THRESHOLD
#define LARGE_AREA_FRINGE 5 // Threshold for large area fringe

// This is used to help calculate ABS_TOUCH_MAJOR
// This is roughly the value of 500 / 10 or 320 / 7
#define PIXELS_PER_X 45
#define PIXELS_PER_Y 50

struct touchpoint {
	// Power or weight of the touch, used for calculating the center point.
	int pw;
	// These store the average of the locations in the digitizer matrix that
	// make up the touch.  Used for calculating the center point.
	float i;
	float j;
	// Tracking ID that is assigned to this touch.
	int tracking_id;
	// Index location of this touch in the previous set of touches.
	int prev_loc;
#if MAX_DELTA_FILTER
	// Direction and distance between this touch and the previous touch.
	float direction;
	int distance;
#endif
	// Size of the touch area.
	int touch_major;
	// X and Y locations of the touch.  These values may have been changed by a
	// filter.
	int x;
	int y;
	// Unfiltered location of the touch.
	int unfiltered_x;
	int unfiltered_y;
	// The highest value found in the digitizer matrix of this touch area.
	int highest_val;
	// Delay count for touches that do not have a very high highest_val.
	int touch_delay;
#if HOVER_DEBOUNCE_FILTER
	// Location that we are tracking for hover debounce
	int hover_x;
	int hover_y;
	int hover_delay;
#endif
};

// This array contains the current touches (tpoint), previous touches
// (prevtpoint) and the touches from 2 times ago (prev2tpoint)
struct touchpoint tp[3][MAX_TOUCH];
// These indexes locate the appropriate set of touches in tp
int tpoint, prevtpoint, prev2tpoint;

struct touchpoint start_tp, finish_tp;

// Contains all of the data from the digitizer
unsigned char matrix[X_AXIS_POINTS][Y_AXIS_POINTS];
// Indicates if a point in the digitizer matrix has already been scanned.
int invalid_matrix[X_AXIS_POINTS][Y_AXIS_POINTS];

void determine_area_loc_fringe(float *isum, float *jsum, int *tweight, int i,
	int j, int cur_touch_id){
	float powered;

	// Set fringe point to used for this touch point
	invalid_matrix[i][j] = cur_touch_id;

	// Track touch values to help determine the pixel x, y location
	powered = pow(matrix[i][j], 1.5);
	*tweight += powered;
	*isum += powered * i;
	*jsum += powered * j;

	// Check the nearby points to see if they are above LARGE_AREA_FRINGE
	// but still decreasing in value to ensure that they are part of the same
	// touch and not a nearby, pinching finger.
	if (i > 0 && invalid_matrix[i-1][j] != cur_touch_id)
	{
		if (matrix[i-1][j] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j,
				cur_touch_id);
	}
	if (i < X_AXIS_MINUS1 && invalid_matrix[i+1][j] != cur_touch_id)
	{
		if (matrix[i+1][j] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j,
				cur_touch_id);
	}
	if (j > 0 && invalid_matrix[i][j-1] != cur_touch_id) {
		if (matrix[i][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i, j - 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && invalid_matrix[i][j+1] != cur_touch_id)
	{
		if (matrix[i][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i, j + 1,
				cur_touch_id);
	}
	if (i > 0 && j > 0 && invalid_matrix[i-1][j-1] != cur_touch_id)
	{
		if (matrix[i-1][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j - 1,
				cur_touch_id);
	}
	if (i < X_AXIS_MINUS1 && j > 0 && invalid_matrix[i+1][j-1] != cur_touch_id)
	{
		if (matrix[i+1][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j - 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && i > 0 && invalid_matrix[i-1][j+1] != cur_touch_id)
	{
		if (matrix[i-1][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j + 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && i < X_AXIS_MINUS1 &&
		invalid_matrix[i+1][j+1] != cur_touch_id)
	{
		if (matrix[i+1][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j + 1,
				cur_touch_id);
	}
}

void determine_area_loc(float *isum, float *jsum, int *tweight, int i, int j,
	int *mini, int *maxi, int *minj, int *maxj, int cur_touch_id,
	int *highest_val){
	float powered;

	// Invalidate this touch point so that we don't process it later
	invalid_matrix[i][j] = cur_touch_id;

	// Track the size of the touch for TOUCH_MAJOR
	if (i < *mini)
		*mini = i;
	if (i > *maxi)
		*maxi = i;
	if (j < *minj)
		*minj = j;
	if (j > *maxj)
		*maxj = j;

	// Track the highest value of the touch to determine which threshold
	// applies.
	if (matrix[i][j] > *highest_val)
		*highest_val = matrix[i][j];

	// Track touch values to help determine the pixel x, y location
	powered = pow(matrix[i][j], 1.5);
	*tweight += powered;
	*isum += powered * i;
	*jsum += powered * j;

	// Check nearby points to see if they are above LARGE_AREA_UNPRESS
	// or if they are above LARGE_AREA_FRINGE but the next nearby point is
	// decreasing in value.  If the value is not decreasing and below
	// LARGE_AREA_UNPRESS then we have 2 fingers pinched close together.
	if (i > 0 && invalid_matrix[i-1][j] != cur_touch_id)
	{
		if (matrix[i-1][j] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i - 1, j, mini, maxi, minj,
			maxj, cur_touch_id, highest_val);
		else if (matrix[i-1][j] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j,
				cur_touch_id);
	}
	if (i < X_AXIS_MINUS1 && invalid_matrix[i+1][j] != cur_touch_id)
	{
		if (matrix[i+1][j] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i + 1, j, mini, maxi, minj,
			maxj, cur_touch_id, highest_val);
		else if (matrix[i+1][j] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j,
				cur_touch_id);
	}
	if (j > 0 && invalid_matrix[i][j-1] != cur_touch_id)
	{
		if (matrix[i][j-1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i, j - 1, mini, maxi, minj,
				maxj, cur_touch_id, highest_val);
		else if (matrix[i][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i, j - 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && invalid_matrix[i][j+1] != cur_touch_id)
	{
		if (matrix[i][j+1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i, j + 1, mini, maxi, minj,
				maxj, cur_touch_id, highest_val);
		else if (matrix[i][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i, j + 1,
				cur_touch_id);
	}
	if (i > 0 && j > 0 && invalid_matrix[i-1][j-1] != cur_touch_id)
	{
		if (matrix[i-1][j-1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i - 1, j - 1, mini, maxi,
				minj, maxj, cur_touch_id, highest_val);
		else if (matrix[i-1][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j - 1,
				cur_touch_id);
	}
	if (i < X_AXIS_MINUS1 && j > 0 && invalid_matrix[i+1][j-1] != cur_touch_id)
	{
		if (matrix[i+1][j-1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i + 1, j - 1, mini, maxi,
				minj, maxj, cur_touch_id, highest_val);
		else if (matrix[i+1][j-1] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j-1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j - 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && i > 0 && invalid_matrix[i-1][j+1] != cur_touch_id)
	{
		if (matrix[i-1][j+1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i - 1, j + 1, mini, maxi,
				minj, maxj, cur_touch_id, highest_val);
		else if (matrix[i-1][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i-1][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i - 1, j + 1,
				cur_touch_id);
	}
	if (j < Y_AXIS_MINUS1 && i < X_AXIS_MINUS1 &&
		invalid_matrix[i+1][j+1] != cur_touch_id)
	{
		if (matrix[i+1][j+1] >= LARGE_AREA_UNPRESS)
			determine_area_loc(isum, jsum, tweight, i + 1, j + 1, mini, maxi,
				minj, maxj, cur_touch_id, highest_val);
		else if (matrix[i+1][j+1] >= LARGE_AREA_FRINGE &&
			matrix[i+1][j+1] < matrix[i][j])
			determine_area_loc_fringe(isum, jsum, tweight, i + 1, j + 1,
				cur_touch_id);
	}
}

#include <linux/input.h>
#include <linux/uinput.h>
#define UINPUT_LOCATION "/dev/uinput"
//#define EVENT_DEBUG 1
int uinput_fd;

int send_uevent(int fd, __u16 type, __u16 code, __s32 value)
{
	struct input_event event;

#if EVENT_DEBUG
	char ctype[20], ccode[20];
	switch (type) {
		case EV_ABS:
			strcpy(ctype, "EV_ABS");
			break;
		case EV_KEY:
			strcpy(ctype, "EV_KEY");
			break;
		case EV_SYN:
			strcpy(ctype, "EV_SYN");
			break;
	}
	switch (code) {
		case ABS_MT_SLOT:
			strcpy(ccode, "ABS_MT_SLOT");
			break;
		case ABS_MT_TRACKING_ID:
			strcpy(ccode, "ABS_MT_TRACKING_ID");
			break;
		case ABS_MT_TOUCH_MAJOR:
			strcpy(ccode, "ABS_MT_TOUCH_MAJOR");
			break;
		case ABS_MT_POSITION_X:
			strcpy(ccode, "ABS_MT_POSITION_X");
			break;
		case ABS_MT_POSITION_Y:
			strcpy(ccode, "ABS_MT_POSITION_Y");
			break;
		case SYN_MT_REPORT:
			strcpy(ccode, "SYN_MT_REPORT");
			break;
		case SYN_REPORT:
			strcpy(ccode, "SYN_REPORT");
			break;
		case BTN_TOUCH:
			strcpy(ccode, "BTN_TOUCH");
			break;
	}
	printf("event type: '%s' code: '%s' value: %i \n", ctype, ccode, value);
#endif

	memset(&event, 0, sizeof(event));
	event.type = type;
	event.code = code;
	event.value = value;

	if (write(fd, &event, sizeof(event)) != sizeof(event)) {
		fprintf(stderr, "Error on send_event %d", sizeof(event));
		return -1;
	}

	return 0;
}

int first_touch = 1;

int calc_point(void)
{
	int i, j, k;
	int tweight = 0;
	int tpc = 0;
	float isum = 0, jsum = 0;
	float avgi, avgj;
	static int previoustpc, tracking_id = 0;
#if DEBOUNCE_FILTER
	int new_debounce_touch = 0;
	static int initialx, initialy;
#endif

	if (tp[tpoint][0].x < -20) {
		// We had a total liftoff
		previoustpc = 0;
#if DEBOUNCE_FILTER
		new_debounce_touch = 1;
#endif
	} else {
		// Re-assign array indexes
		prev2tpoint = prevtpoint;
		prevtpoint = tpoint;
		tpoint++;
		if (tpoint > 2)
			tpoint = 0;
	}

	// Scan the digitizer data and generate a list of touches
	memset(&invalid_matrix, 0, sizeof(invalid_matrix));
	for(i=0; i < X_AXIS_POINTS; i++) {
		for(j=0; j < Y_AXIS_POINTS; j++) {
#if RAW_DATA_DEBUG
			if (matrix[i][j] < RAW_DATA_THRESHOLD)
				printf("   ");
			else
				printf("%2.2X ", matrix[i][j]);
#endif
			if (tpc < MAX_TOUCH && matrix[i][j] > touch_continue_thresh &&
				!invalid_matrix[i][j]) {

				isum = 0;
				jsum = 0;
				tweight = 0;
				int mini = i, maxi = i, minj = j, maxj = j;
				int highest_val = matrix[i][j];
				determine_area_loc(&isum, &jsum, &tweight, i, j, &mini,
					&maxi, &minj, &maxj, tpc + 1, &highest_val);

				avgi = isum / (float)tweight;
				avgj = jsum / (float)tweight;
				maxi = maxi - mini;
				maxj = maxj - minj;

				tp[tpoint][tpc].pw = tweight;
				tp[tpoint][tpc].i = avgi;
				tp[tpoint][tpc].j = avgj;
				tp[tpoint][tpc].touch_major = MAX(maxi * PIXELS_PER_X, maxj * PIXELS_PER_Y);
				tp[tpoint][tpc].tracking_id = -1;				
				tp[tpoint][tpc].x = tp[tpoint][tpc].i *	X_LOCATION_VALUE;
				tp[tpoint][tpc].y = tp[tpoint][tpc].j *	Y_LOCATION_VALUE;
				if (tp[tpoint][tpc].x < 0)
					tp[tpoint][tpc].x = 0;
				if (tp[tpoint][tpc].y < 0)
					tp[tpoint][tpc].y = 0;
					
				tp[tpoint][tpc].unfiltered_x = tp[tpoint][tpc].x;
				tp[tpoint][tpc].unfiltered_y = tp[tpoint][tpc].y;
				tp[tpoint][tpc].highest_val = highest_val;
				tp[tpoint][tpc].touch_delay = 0;
#if HOVER_DEBOUNCE_FILTER
				tp[tpoint][tpc].hover_x = tp[tpoint][tpc].x;
				tp[tpoint][tpc].hover_y = tp[tpoint][tpc].y;
				tp[tpoint][tpc].hover_delay = HOVER_DEBOUNCE_DELAY;
#endif

				//printf("touch %d, %d\n", tp[tpoint][tpc].x, tp[tpoint][tpc].y);
				tpc++;
			}
		}
	}
	
	for (k = 0; k < tpc; k++) {
		if (tp[tpoint][k].highest_val && !tp[tpoint][k].touch_delay) {
#if EVENT_DEBUG
			printf("send event for tracking ID: %i\n",
				tp[tpoint][k].tracking_id);
#endif
			if (first_touch) {
				first_touch = 0;
				start_tp = tp[tpoint][k];
			} else {
				finish_tp = tp[tpoint][k];
			}
			if (tp[tpoint][k].x > X_SCREEN || tp[tpoint][k].y > Y_SCREEN)
				continue;
			send_uevent(uinput_fd, EV_ABS, ABS_MT_POSITION_X, tp[tpoint][k].x);
			send_uevent(uinput_fd, EV_ABS, ABS_MT_POSITION_Y, tp[tpoint][k].y);
			send_uevent(uinput_fd, EV_KEY, BTN_TOUCH, 1);
			send_uevent(uinput_fd, EV_SYN, SYN_MT_REPORT, 0);
		} else if (tp[tpoint][k].touch_delay) {
			// This touch didn't meet the threshold so we don't report it yet
			tp[tpoint][k].touch_delay--;
		}
	}
	if (tpc > 0) {
		send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
	}
	previoustpc = tpc;
	
	return tpc;
}



void clear_arrays(void)
{
	// Clears array (for after a total liftoff occurs)
	int i, j;
	for (i=0; i<3; i++) {
		for(j=0; j<MAX_TOUCH; j++) {
			tp[i][j].pw = -1000;
			tp[i][j].i = -1000;
			tp[i][j].j = -1000;
			
			tp[i][j].tracking_id = -1;
			tp[i][j].prev_loc = -1;
#if MAX_DELTA_FILTER
			tp[i][j].direction = 0;
			tp[i][j].distance = 0;
#endif
			tp[i][j].touch_major = 0;
			tp[i][j].x = -1000;
			tp[i][j].y = -1000;
			tp[i][j].unfiltered_x = -1000;
			tp[i][j].unfiltered_y = -1000;
			tp[i][j].highest_val = -1000;
			tp[i][j].touch_delay = -1000;
#if HOVER_DEBOUNCE_FILTER
			tp[i][j].hover_x = -1000;
			tp[i][j].hover_y = -1000;
			tp[i][j].hover_delay = HOVER_DEBOUNCE_DELAY;
#endif
		}
	}
}

void liftoff(void)
{
	// Sends liftoff events - nothing is touching the screen
#if EVENT_DEBUG
	printf("liftoff function\n");
#endif
	//send_uevent(uinput_fd, EV_KEY, BTN_TOUCH, 0);
	//send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
	//send_uevent(uinput_fd, EV_ABS, ABS_MT_TOUCH_MAJOR,0);
	//send_uevent(uinput_fd, EV_ABS, ABS_MT_PRESSURE, 0);
	
	send_uevent(uinput_fd, EV_KEY, BTN_TOUCH, 0);
	send_uevent(uinput_fd, EV_SYN, SYN_MT_REPORT, 0);
	send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
	
	first_touch = 1;
	
	if (start_tp.y > Y_SCREEN) {
		if (finish_tp.y < Y_SCREEN) {
			send_uevent(uinput_fd, EV_KEY, KEY_HOME, 1);
			send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
			send_uevent(uinput_fd, EV_KEY, KEY_HOME, 0);
			send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
		} else {
			if (finish_tp.x < start_tp.x) {
				send_uevent(uinput_fd, EV_KEY, KEY_BACK, 1);
				send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
				send_uevent(uinput_fd, EV_KEY, KEY_BACK, 0);
				send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
			} else {			
				send_uevent(uinput_fd, EV_KEY, KEY_MENU, 1);
				send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
				send_uevent(uinput_fd, EV_KEY, KEY_MENU, 0);
				send_uevent(uinput_fd, EV_SYN, SYN_REPORT, 0);
			}
		}
	}
}

#define LIFTOFF_TIMEOUT 25000
void irq_message()
{
	int ret;	
	fd_set rdfds;
	struct timeval tv;
	char buf[32];
	int i;
	int x, y;
	struct spi_ioc_transfer ioc;	
	int need_liftoff = 0;
	
	FD_ZERO(&rdfds);
	FD_SET(ctp_fd, &rdfds);
	
	ioc.tx_buf = NULL;
	ioc.rx_buf = rx_buf;
	ioc.len = 132;
	ioc.speed_hz = 1600000;
	ioc.delay_usecs = 0;
	ioc.bits_per_word = 8;
	ioc.cs_change = 0;
	ioc.pad = 0;
	
	while (1) {
		FD_ZERO(&rdfds);
		FD_SET(ctp_fd, &rdfds);
		tv.tv_sec = 0;
		tv.tv_usec = LIFTOFF_TIMEOUT;
		ret = select(ctp_fd + 1, &rdfds, NULL, NULL, &tv);
		if(ret < 0)
			perror("select");
		else if(ret == 0) {
			if (need_liftoff) {
//#if EVENT_DEBUG
				//printf("timeout called liftoff\n");
//#endif
				liftoff();
				clear_arrays();
				need_liftoff = 0;
			}
		}
		else {
			if(FD_ISSET(ctp_fd, &rdfds)) {
				read(ctp_fd, buf, 2);
				
				ret = write(ss_fd, "0", 1);
				ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &ioc);
				ret = write(ss_fd, "1", 1);
				
				for (y = 0; y < 10 ; y++) {
					for (x = 0; x < 7 ; x++) {
						matrix[x][y] = rx_buf[x * 10 + y + 2];
					}
				}
				if (!calc_point()) {
				// Sometimes there's data but no valid touches due to threshold
					if (need_liftoff) {
						liftoff();
						clear_arrays();
						need_liftoff = 0;
					} 
				} else {
						need_liftoff = 1;
				}
			}
		}
	}
}

void init_uinput()
{
	struct uinput_user_dev device;

	memset(&device, 0, sizeof(device));

	uinput_fd=open(UINPUT_LOCATION, O_WRONLY);
	strcpy(device.name,"HPVeer");

	device.id.bustype = BUS_VIRTUAL;
	device.id.vendor = 1;
	device.id.product = 1;
	device.id.version = 1;
	
	//device.absmax[ABS_MT_PRESSURE] = 255;
	//device.absmin[ABS_MT_PRESSURE] = 0;
	//device.absmax[ABS_MT_TOUCH_MAJOR] = 255;
	//device.absmin[ABS_MT_TOUCH_MAJOR] = 0;
	
	device.absmax[ABS_MT_POSITION_X] = X_SCREEN;
	device.absmax[ABS_MT_POSITION_Y] = Y_SCREEN;
	device.absmin[ABS_MT_POSITION_X] = 0;
	device.absmin[ABS_MT_POSITION_Y] = 0;
	
	device.absfuzz[ABS_MT_POSITION_X] = 2;
	device.absflat[ABS_MT_POSITION_X] = 0;
	device.absfuzz[ABS_MT_POSITION_Y] = 1;
	device.absflat[ABS_MT_POSITION_Y] = 0;


	if (write(uinput_fd,&device,sizeof(device)) != sizeof(device))
		fprintf(stderr, "error setup\n");
		
	if (ioctl(uinput_fd,UI_SET_EVBIT, EV_KEY) < 0)
		fprintf(stderr, "error evbit key\n");

	if (ioctl(uinput_fd,UI_SET_EVBIT,EV_ABS) < 0)
		fprintf(stderr, "error evbit rel\n");
	
	if (ioctl(uinput_fd,UI_SET_KEYBIT, BTN_TOUCH) < 0)
		fprintf(stderr, "error keybit key\n");
		
	if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_BACK) < 0)
		fprintf(stderr, "error keybit key\n");
	if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_MENU) < 0)
		fprintf(stderr, "error keybit key\n");
	if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_HOME) < 0)
		fprintf(stderr, "error keybit key\n");
		
	if (ioctl(uinput_fd,UI_SET_EVBIT, EV_SYN) < 0)
		fprintf(stderr, "error evbit key\n");

	//if (ioctl(uinput_fd,UI_SET_ABSBIT,ABS_MT_TOUCH_MAJOR) < 0)
	//	fprintf(stderr, "error tool rel\n");

	if (ioctl(uinput_fd,UI_SET_ABSBIT,ABS_MT_POSITION_X) < 0)
		fprintf(stderr, "error tool rel\n");

	if (ioctl(uinput_fd,UI_SET_ABSBIT,ABS_MT_POSITION_Y) < 0)
		fprintf(stderr, "error tool rel\n");
	
	if (ioctl(uinput_fd,UI_SET_ABSBIT,ABS_MT_PRESSURE) < 0)
		fprintf(stderr, "error tool rel\n");

	if (ioctl(uinput_fd,UI_DEV_CREATE) < 0)
		fprintf(stderr, "error create\n");
}



int main()
{
	init_uinput();
	enable_ctp();
	usleep(10000);
	init_ctp();
	liftoff();
	irq_message();
	return 0;
}

