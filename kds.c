#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/xarray.h>
#include <linux/bitmap.h>

static char *int_str = NULL;
static size_t str_len = 0;

module_param(int_str, charp, 0);
MODULE_PARM_DESC(int_str, "string representation of arbitrary number of integers between 0 to 1000");

/* linked list area start */
struct kds_linked_list
{
	int data;
	struct list_head list;
};

LIST_HEAD(kds_list_head);
/* linked list area end */

/* red black tree area start */
struct rb_root kds_rb_root = RB_ROOT;

struct kds_rb_node
{
	int data;
	struct rb_node node;
};

static void __kds_rb_insert(struct kds_rb_node *rb_node)
{
	struct rb_node **link = &kds_rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct kds_rb_node *entry;

	while (*link)
	{
		parent = *link;
		entry = rb_entry(parent, struct kds_rb_node, node);

		if (rb_node->data < entry->data)
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}

	rb_link_node(&rb_node->node, parent, link);
	rb_insert_color(&rb_node->node, &kds_rb_root);
}
/* red black tree area end */

/* hash table area start */
struct kds_hash_table
{
	DECLARE_HASHTABLE(table, 10);
};

struct kds_ht_node
{
	int data;
	struct hlist_node node;
};
/* hash table area end */

/* radix tree area start */
RADIX_TREE(kds_radix_tree, GFP_KERNEL);
/* radix tree area end */

/* xarray area start */
DEFINE_XARRAY(kds_xarray);
/* xarray area end */

/* bitmap area start */
DECLARE_BITMAP(kds_bitmap, 10);
/* bitmap area end */

static void kds_do_bitmap(void)
{
	char *token, *cursor;
	u8 bit;

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		printk(KERN_INFO "Begin bitmap test:\n");

		while ((token = strsep(&cursor, " ")))
		{
			long num;
			int inum, i, c = 0;

			if (kstrtol(token, 10, &num))
				continue;

			bitmap_zero(kds_bitmap, 10);
			inum = (int)num;
			for (i = 9; i >= 0; i--)
			{
				int res = inum >> i;
				if (res & 1)
					set_bit(9 - c, kds_bitmap);
				c++;
			}

			printk(KERN_INFO "bits that are turned on for %d:\n", inum);
			for_each_set_bit(bit, kds_bitmap, 10)
			{
				printk(KERN_INFO "%u\n", bit);
			}
			printk(KERN_INFO "finished one iteration\n");
		}

		/* clear all bits */
		bitmap_zero(kds_bitmap, 10);

		kfree(cursor);

		printk(KERN_INFO "End bitmap test\n");
	}
}

static void kds_do_xarray(void)
{
	char *token, *cursor;

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		int *result;
		unsigned long index;
		printk(KERN_INFO "Begin xarray test:\n");

		while ((token = strsep(&cursor, " ")))
		{
			long num;
			int *new_node;

			if (kstrtol(token, 10, &num))
				continue;

			if ((new_node = kmalloc(sizeof(*new_node), GFP_KERNEL)))
			{
				*new_node = (int)num;
				xa_store(&kds_xarray, *new_node, new_node, GFP_KERNEL);
			}
		}

		xa_for_each(&kds_xarray, index, result)
		{
			printk(KERN_INFO "%d\n", *result);

			if (*result % 2)
			{
				xa_set_mark(&kds_xarray, index, XA_MARK_0);
			}
		}

		printk(KERN_INFO "Begin xarray odd number test:\n");
		xa_for_each_marked(&kds_xarray, index, result, XA_MARK_0)
		{
			printk(KERN_INFO "%d\n", *result);

			if (*result % 2)
			{
				xa_set_mark(&kds_xarray, index, XA_MARK_0);
			}
		}
		printk(KERN_INFO "End xarray odd number test:\n");

		printk(KERN_INFO "Removing xarray entries...\n");
		xa_for_each(&kds_xarray, index, result)
		{
			kfree(xa_erase(&kds_xarray, *result));
		}

		xa_destroy(&kds_xarray);

		kfree(cursor);

		printk(KERN_INFO "End xarray test\n");
	}
}

static void kds_do_radix_tree(void)
{
	char *token, *cursor;

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		int **results;
		int nfound, i, num_len = 0;
		printk(KERN_INFO "Begin radix tree test:\n");

		while ((token = strsep(&cursor, " ")))
		{
			long num;
			int *new_node;

			if (kstrtol(token, 10, &num))
				continue;

			if ((new_node = kmalloc(sizeof(*new_node), GFP_KERNEL)))
			{
				*new_node = (int)num;
				radix_tree_insert(&kds_radix_tree, *new_node, new_node);
				num_len++;
			}
		}

		if ((results = kmalloc(sizeof(int *) * num_len, GFP_KERNEL)) &&
		    (nfound = radix_tree_gang_lookup(&kds_radix_tree, (void **)results, 0,
						     num_len)))
		{
			int nfound2, odd_count = 0;
			int **results2;

			for (i = 0; i < nfound; i++)
			{
				int result = *results[i];
				printk(KERN_INFO "%d\n", result);

				if (result % 2)
				{
					radix_tree_tag_set(&kds_radix_tree, result, 1);
					odd_count++;
				}
			}

			if ((results2 = kmalloc(sizeof(int *) * odd_count, GFP_KERNEL)) &&
			    (nfound2 = radix_tree_gang_lookup_tag(&kds_radix_tree, (void **)results2, 0,
								  odd_count, 1)))
			{
				printk(KERN_INFO "Start radix tree odd number test\n");
				for (i = 0; i < nfound2; i++)
				{
					int result = *results2[i];
					printk(KERN_INFO "%d\n", result);
				}
				printk(KERN_INFO "End radix tree odd number test\n");
			}

			kfree(results2);
		}

		printk(KERN_INFO "Removing radix tree nodes...\n");
		for (i = 0; i < nfound; i++)
		{
			int result = *results[i];
			kfree(radix_tree_delete(&kds_radix_tree, result));
		}

		kfree(results);
		kfree(cursor);

		printk(KERN_INFO "End radix tree test\n");
	}
}

static void kds_do_hash_table(void)
{
	char *token, *cursor;
	struct kds_hash_table *ht;

	if ((ht = kmalloc(sizeof(*ht), GFP_KERNEL)) == NULL)
		return;

	hash_init(ht->table);

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		int bkt;
		struct hlist_node *temp;
		struct kds_ht_node *cursor2;

		printk(KERN_INFO "Begin hash table test:\n");

		while ((token = strsep(&cursor, " ")))
		{
			long num;
			struct kds_ht_node *new_node;

			if (kstrtol(token, 10, &num))
				continue;

			if ((new_node = kmalloc(sizeof(*new_node), GFP_KERNEL)))
			{
				new_node->data = (int)num;
				hash_add(ht->table, &new_node->node, new_node->data);
			}
		}

		hash_for_each_safe(ht->table, bkt, temp, cursor2, node)
		{
			struct hlist_node *temp2;
			struct kds_ht_node *cursor3;

			printk(KERN_INFO "%d\n", cursor2->data);

			hash_for_each_possible_safe(ht->table, cursor3, temp2, node, cursor2->data)
			{
				printk(KERN_INFO "hash table lookup prints: %d\n", cursor3->data);
			}

			hash_del(&cursor2->node);
			kfree(cursor2);
		}

		kfree(cursor);

		printk(KERN_INFO "End hash table test\n");
	}

	kfree(ht);
}

static void kds_do_rb_tree(void)
{
	char *token, *cursor;
	struct rb_node *cursor2;

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		printk(KERN_INFO "Begin red black tree test:\n");

		while ((token = strsep(&cursor, " ")))
		{
			long num;
			struct kds_rb_node *new_node;

			if (kstrtol(token, 10, &num))
				continue;

			if ((new_node = kmalloc(sizeof(*new_node), GFP_KERNEL)))
			{
				new_node->data = (int)num;
				__kds_rb_insert(new_node);
			}
		}

		/* print data from a red black tree, then destruct and free it */
		cursor2 = rb_first(&kds_rb_root);
		while (cursor2)
		{
			struct rb_node *temp;
			struct kds_rb_node *entry;

			entry = rb_entry(cursor2, struct kds_rb_node, node);
			printk(KERN_INFO "%d\n", entry->data);
			temp = cursor2;
			cursor2 = rb_next(cursor2);
			rb_erase(temp, &kds_rb_root);
			kfree(rb_entry(temp, struct kds_rb_node, node));
		}

		kfree(cursor);

		printk(KERN_INFO "End red black tree test\n");
	}
}

static void kds_do_linked_list(void)
{
	char *token, *cursor;
	struct kds_linked_list *cursor2, *temp;

	if ((cursor = kmalloc(str_len + 1, GFP_KERNEL)) && strncpy(cursor, int_str, str_len))
	{
		printk(KERN_INFO "Begin linked list test:\n");

		/* create a linked list */
		while ((token = strsep(&cursor, " ")))
		{
			long num;
			struct kds_linked_list *linked_list;

			if (kstrtol(token, 10, &num))
				continue;

			if ((linked_list = kmalloc(sizeof(*linked_list), GFP_KERNEL)))
			{
				linked_list->data = (int)num;
				INIT_LIST_HEAD(&linked_list->list);
				list_add_tail(&linked_list->list, &kds_list_head);
			}
		}

		/* print data from a linked list, then destruct and free it */
		list_for_each_entry_safe(cursor2, temp, &kds_list_head, list)
		{
			printk(KERN_INFO "%d\n", cursor2->data);
			list_del(&cursor2->list);
			kfree(cursor2);
		}

		kfree(cursor);

		printk(KERN_INFO "End linked list test\n");
	}
}

static int __init kds_init(void)
{
	char *token, *cursor;
	printk(KERN_INFO "Module loaded ...\n");

	if (int_str && (str_len = strlen(int_str)) && (cursor = kmalloc(str_len + 1, GFP_KERNEL)) &&
	    strncpy(cursor, int_str, str_len))
	{
		while ((token = strsep(&cursor, " ")))
		{
			long num;

			if (kstrtol(token, 10, &num))
				continue;

			printk(KERN_INFO "%d\n", (int)num);
		}
		kfree(cursor);

		kds_do_linked_list();
		kds_do_rb_tree();
		kds_do_hash_table();
		kds_do_radix_tree();
		kds_do_xarray();
		kds_do_bitmap();
	}

	return 0;
}

static void __exit kds_exit(void)
{
	printk(KERN_INFO "Module exiting ...\n");
}

module_init(kds_init);
module_exit(kds_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Pei Liu");
MODULE_DESCRIPTION("Kernel Data Structure module");