/*
 * Kernel Wakelock Block driver
 *
 * Copyright (c) 2018 Sonsation <jhson9304@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/wakelock_blocker.h>

static char wakelock_item_list[MAX_NAME_LEN];
static LIST_HEAD(wakelock_list);
static bool enabled = DEFAULT_ENABLED;

bool check_wakelock(struct wakeup_source *ws) {

	struct wake_item *wakelock, *n;

	if(!enabled)
		return false;

	list_for_each_entry_safe(wakelock,n,&wakelock_list, list) {

		if(memcmp(wakelock->wake_name, ws->name, wakelock->len) == 0) {

			if(wakelock->wake_count == SIZE_MAX)
				wakelock->wake_count = 0;	

			wakelock->wake_count++;

			return true;
		}
	}

	return false;

}

static int item_to_list(void) {

	char *index, *start, *addr;
    
	index = wakelock_item_list;
	start = wakelock_item_list;

	while(*index != '\0') {

		if(*(index+1) == ' ' || *(index+1) == '\0') {

			struct wake_item *wake = kzalloc(sizeof(struct wake_item), GFP_KERNEL);

			unsigned int len = 0;

			for(addr = start ; addr < index+1 ; addr++) {

				if(len > MAX_NAME_LEN - 1)
					return -EINVAL;

				len += sprintf(wake->wake_name + len, "%c", *addr);
			}

			wake->len = strlen(wake->wake_name);
			list_add(&wake->list, &wakelock_list);
			start = index +2;	
		}
		index++;
	}

	return false;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr, char *buf) {

	unsigned int len = 0;
	struct wake_item *wakelock, *n;

	if(!list_empty(&wakelock_list)) {

		list_for_each_entry_safe(wakelock, n, &wakelock_list, list) {
			len += scnprintf(buf+len, PAGE_SIZE, "name : %s \t\t block count : %lu \n", wakelock->wake_name, wakelock->wake_count);
		}

	} else {
		
		len = scnprintf(buf, PAGE_SIZE, "%s \n", "could not create debug");

	}

	return len;	
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return scnprintf(buf, PAGE_SIZE, "%d", enabled);	 
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {

	unsigned long val = 0;

	int ret = kstrtoul(buf, 0, &val);

	if (ret < 0)
		return ret;

 	enabled = !!val;

	return len;
}

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return scnprintf(buf, PAGE_SIZE, "%s", MAJOR_VERSION);
}

static ssize_t list_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return scnprintf(buf, PAGE_SIZE, "%s", wakelock_item_list);
}
 
static ssize_t list_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {

	int ret = 0;
	struct wake_item *wakelock, *n;

	if (len > MAX_NAME_LEN)
		return -EINVAL;
	
	memset(wakelock_item_list, 0, sizeof(wakelock_item_list));
	memcpy(wakelock_item_list, buf, len);

	list_for_each_entry_safe(wakelock, n, &wakelock_list, list) {
		list_del(&wakelock->list);
		kfree(wakelock);
	}

	ret = item_to_list();

	if (ret)
		pr_err("Failed to create list \n");
    
	return len;
}

static DEVICE_ATTR(enabled, 0660, enabled_show, enabled_store);
static DEVICE_ATTR(list, 0660, list_show, list_store);
static DEVICE_ATTR(version, 0640, version_show, NULL);
static DEVICE_ATTR(debug, 0640, debug_show, NULL);

static struct attribute *wakelock_blocker_attrs[] = {
	&dev_attr_enabled.attr,
	&dev_attr_list.attr,
	&dev_attr_version.attr,
	&dev_attr_debug.attr,	
	NULL,
};

static struct attribute_group wakelock_blocker_control_group = {
	.attrs = wakelock_blocker_attrs,
};

static struct miscdevice wakelock_blocker_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wakelock_blocker",
};

static int __init blocker_sysfs_init(void) {

        int ret = 0;

	memcpy(wakelock_item_list, DEFAULT_LIST, sizeof(DEFAULT_LIST));
	
	ret = item_to_list();

	if (ret) {
		pr_err("Failed to create list \n");
	}
 
	misc_register(&wakelock_blocker_control_device);
	if (sysfs_create_group(&wakelock_blocker_control_device.this_device->kobj,
				&wakelock_blocker_control_group) < 0) {
		printk("failed to create sys fs object.\n");
		return 0;
	}

	return 0;
}

static void __exit blocker_sysfs_exit(void) {

	struct wake_item *wakelock, *n;

	list_for_each_entry_safe(wakelock, n, &wakelock_list, list) {
		list_del(&wakelock->list);
		kfree(wakelock);
	}

	sysfs_remove_group(&wakelock_blocker_control_device.this_device->kobj,
                           &wakelock_blocker_control_group);

}

module_init(blocker_sysfs_init);
module_exit(blocker_sysfs_exit);

MODULE_AUTHOR("Sonsation <jhson9304@gmail.com>");
MODULE_DESCRIPTION("Kernel Wakelock Block driver");
MODULE_LICENSE("GPL");

