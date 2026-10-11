#include "common.h"

G_DEFINE_TYPE(LogicalScrollPane, logical_scroll_pane, GTK_TYPE_BIN)

static void
logical_scroll_pane_queue_child_draw(GtkAdjustment *adjustment, LogicalScrollPane *this)
{
	(void)adjustment;
	if (this->child)
		gtk_widget_queue_draw(this->child);
}

static void
logical_scroll_pane_configure_adjustment(GtkAdjustment *adjustment, int content_size, int page_size)
{
	double value, limit;

	page_size = MAX(page_size, 1);

	limit = (double)MAX(content_size - page_size, 0);
	value = MAX(0.0, adjustment->value);
	value = MIN(value, limit);

	gtk_adjustment_configure(adjustment, value, 0, MAX(content_size, page_size),
	                         page_size * 0.1, page_size * 0.9, page_size);
}

static void
logical_scroll_pane_destroy(GtkObject *this_)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);

	if (this->child)       gtk_widget_unparent(this->child);
	if (this->hscrollbar)  gtk_widget_unparent(this->hscrollbar);
	if (this->vscrollbar)  gtk_widget_unparent(this->vscrollbar);
	if (this->hadjustment) g_object_unref(this->hadjustment);
	if (this->vadjustment) g_object_unref(this->vadjustment);

	this->child = NULL;
	this->hscrollbar = NULL;
	this->vscrollbar = NULL;
	this->hadjustment = NULL;
	this->vadjustment = NULL;

	GTK_OBJECT_CLASS(logical_scroll_pane_parent_class)->destroy(this_);
}

static void
logical_scroll_pane_size_request(GtkWidget *this_, GtkRequisition *requisition)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);
	GtkRequisition hreq;
	GtkRequisition vreq;

	requisition->width  = hreq.width  = vreq.width  = 0;
	requisition->height = hreq.height = vreq.height = 0;

	if (this->hscrollbar && this->hscrollbar_visible) gtk_widget_size_request(this->hscrollbar, &hreq);
	if (this->vscrollbar && this->vscrollbar_visible) gtk_widget_size_request(this->vscrollbar, &vreq);

	if (this->vscrollbar_visible) requisition->width  += vreq.width;
	if (this->hscrollbar_visible) requisition->height += hreq.height;

	requisition->width  = MAX(requisition->width,  hreq.width);
	requisition->height = MAX(requisition->height, vreq.height);
}

static void
logical_scroll_pane_size_allocate(GtkWidget *this_, GtkAllocation *allocation)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);

	GtkRequisition hreq;
	GtkRequisition vreq;

	GtkAllocation child_allocation;
	GtkAllocation hscrollbar_allocation;
	GtkAllocation vscrollbar_allocation;

	int content_width;
	int content_height;
	double x, y, max;

	this_->allocation = *allocation;

	x = this->hadjustment->value + 0.5 * this->hadjustment->page_size;
	y = this->vadjustment->value + 0.5 * this->vadjustment->page_size;

	hreq.width = hreq.height = 0;
	vreq.width = vreq.height = 0;

	if (this->hscrollbar && this->hscrollbar_visible) gtk_widget_size_request(this->hscrollbar, &hreq);
	if (this->vscrollbar && this->vscrollbar_visible) gtk_widget_size_request(this->vscrollbar, &vreq);

	content_width = allocation->width;
	if (this->vscrollbar_visible)
		content_width -= vreq.width;
	content_width = MAX(content_width, 1);

	content_height = allocation->height;
	if (this->hscrollbar_visible)
		content_height -= hreq.height;
	content_height = MAX(content_height, 1);

	logical_scroll_pane_configure_adjustment(this->hadjustment, this->content_width, content_width);
	logical_scroll_pane_configure_adjustment(this->vadjustment, this->content_height, content_height);

	max = this->hadjustment->upper - this->hadjustment->page_size;
	x -= 0.5 * this->hadjustment->page_size;
	x = MIN(MAX(this->hadjustment->lower, x), max);
	gtk_adjustment_set_value(this->hadjustment, x);

	max = this->vadjustment->upper - this->vadjustment->page_size;
	y -= 0.5 * this->vadjustment->page_size;
	y = MIN(MAX(this->vadjustment->lower, y), max);
	gtk_adjustment_set_value(this->vadjustment, y);

	if (this->child && GTK_WIDGET_VISIBLE(this->child)) {
		child_allocation.x = allocation->x;
		child_allocation.y = allocation->y;
		child_allocation.width = content_width;
		child_allocation.height = content_height;
		gtk_widget_size_allocate(this->child, &child_allocation);
	}

	if (this->hscrollbar && this->hscrollbar_visible) {
		hscrollbar_allocation.x = allocation->x;
		hscrollbar_allocation.y = allocation->y + content_height;
		hscrollbar_allocation.width = content_width;
		hscrollbar_allocation.height = hreq.height;
		gtk_widget_size_allocate(this->hscrollbar, &hscrollbar_allocation);
	}

	if (this->vscrollbar && this->vscrollbar_visible) {
		vscrollbar_allocation.x = allocation->x + content_width;
		vscrollbar_allocation.y = allocation->y;
		vscrollbar_allocation.width = vreq.width;
		vscrollbar_allocation.height = content_height;
		gtk_widget_size_allocate(this->vscrollbar, &vscrollbar_allocation);
	}
}

static void
logical_scroll_pane_add(GtkContainer *this_, GtkWidget *widget)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);
	if (this->child) {
		g_warning("LogicalScrollPane already has a child");
		return;
	}
	logical_scroll_pane_set_child(this, widget);
}

static void
logical_scroll_pane_remove(GtkContainer *this_, GtkWidget *widget)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);

	if (widget == this->child) {
		this->child = NULL;
		GTK_BIN(this)->child = NULL;
	} else if (widget == this->hscrollbar) {
		this->hscrollbar = NULL;
	} else if (widget == this->vscrollbar) {
		this->vscrollbar = NULL;
	} else {
		g_warning("Widget is not a child of LogicalScrollPane");
		return;
	}

	gtk_widget_unparent(widget);
	gtk_widget_queue_resize(GTK_WIDGET(this));
}

static void
logical_scroll_pane_forall(GtkContainer *this_, int include_internals, GtkCallback callback, void *callback_data)
{
	LogicalScrollPane *this = LOGICAL_SCROLL_PANE(this_);

	if (this->child) (*callback)(this->child, callback_data);

	if (!include_internals)
		return;

	if (this->hscrollbar) (*callback)(this->hscrollbar, callback_data);
	if (this->vscrollbar) (*callback)(this->vscrollbar, callback_data);
}

static void
set_adjustment(LogicalScrollPane *this, GtkAdjustment *adjustment, GtkAdjustment **slot, GtkWidget *scrollbar)
{
	if (!adjustment)
		adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0));

	if (*slot != adjustment) {
		g_object_ref_sink(adjustment);

		if (*slot) {
			g_signal_handlers_disconnect_by_data(*slot, this);
			g_object_unref(*slot);
		}

		*slot = adjustment;
		g_signal_connect(*slot, "value-changed",
		                 G_CALLBACK(&logical_scroll_pane_queue_child_draw), this);
	}

	if (scrollbar)
		gtk_range_set_adjustment(GTK_RANGE(scrollbar), adjustment);
}

static void
logical_scroll_pane_set_adjustment(LogicalScrollPane *this, int horizontal, GtkAdjustment *adjustment)
{
	if (horizontal)
		set_adjustment(this, adjustment, &this->hadjustment, this->hscrollbar);
	else
		set_adjustment(this, adjustment, &this->vadjustment, this->vscrollbar);
}

static void
logical_scroll_pane_class_init(LogicalScrollPaneClass *class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS(class);

	object_class->destroy = &logical_scroll_pane_destroy;

	widget_class->size_request = &logical_scroll_pane_size_request;
	widget_class->size_allocate = &logical_scroll_pane_size_allocate;

	container_class->add = &logical_scroll_pane_add;
	container_class->remove = &logical_scroll_pane_remove;
	container_class->forall = &logical_scroll_pane_forall;
}

static void
logical_scroll_pane_init(LogicalScrollPane *this)
{
	this->child = NULL;
	this->content_width = 1;
	this->content_height = 1;
	this->hscrollbar_visible = 1;
	this->vscrollbar_visible = 1;

	this->hadjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.0,   /* value */
	                                                      0.0,   /* lower */
	                                                      0.0,   /* upper */
	                                                      1.0,   /* step increment */
	                                                      1.0,   /* page increment */
	                                                      0.0)); /* page size */

	this->vadjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0));

	g_object_ref_sink(this->hadjustment);
	g_object_ref_sink(this->vadjustment);
	g_signal_connect(this->hadjustment, "value-changed",
	                 G_CALLBACK(&logical_scroll_pane_queue_child_draw), this);
	g_signal_connect(this->vadjustment, "value-changed",
	                 G_CALLBACK(&logical_scroll_pane_queue_child_draw), this);

	this->hscrollbar = gtk_hscrollbar_new(this->hadjustment);
	this->vscrollbar = gtk_vscrollbar_new(this->vadjustment);

	gtk_widget_set_parent(this->hscrollbar, GTK_WIDGET(this));
	gtk_widget_set_parent(this->vscrollbar, GTK_WIDGET(this));

	gtk_widget_show(this->hscrollbar);
	gtk_widget_show(this->vscrollbar);
}

GtkWidget *
logical_scroll_pane_new(void)
{
	return GTK_WIDGET(g_object_new(LOGICAL_TYPE_SCROLL_PANE, NULL));
}

void
logical_scroll_pane_set_child(LogicalScrollPane *this, GtkWidget *child)
{
	if (child == this->child)
		return;

	if (this->child) {
		gtk_widget_unparent(this->child);
		this->child = NULL;
		GTK_BIN(this)->child = NULL;
	}

	if (child) {
		this->child = child;
		GTK_BIN(this)->child = child;
		gtk_widget_set_parent(child, GTK_WIDGET(this));
		if (GTK_WIDGET_VISIBLE(this))
			gtk_widget_show(child);
	}

	gtk_widget_queue_resize(GTK_WIDGET(this));
}

GtkWidget *
logical_scroll_pane_get_child(LogicalScrollPane *this)
{
	return this->child;
}

void
logical_scroll_pane_set_content_size(LogicalScrollPane *this, int width, int height)
{
	this->content_width = MAX(1, width);
	this->content_height = MAX(1, height);
	logical_scroll_pane_configure_adjustment(this->hadjustment, this->content_width, (int)this->hadjustment->page_size);
	logical_scroll_pane_configure_adjustment(this->vadjustment, this->content_height, (int)this->vadjustment->page_size);
	gtk_widget_queue_resize(GTK_WIDGET(this));
	if (this->child)
		gtk_widget_queue_draw(this->child);
}

static void
set_scrollbar(LogicalScrollPane *this, GtkWidget *scrollbar, GtkWidget **slot, int *scrollbar_visible)
{
	if (scrollbar == *slot)
		return;

	if (scrollbar) {
		if (*slot)
			gtk_widget_unparent(*slot);
		*slot = scrollbar;
		gtk_widget_set_parent(scrollbar, GTK_WIDGET(this));

		if (*scrollbar_visible)
			gtk_widget_show(scrollbar);
	} else {
		if (*slot) {
			gtk_widget_unparent(*slot);
			*slot = NULL;
		}
	}

	gtk_widget_queue_resize(GTK_WIDGET(this));
}

void
logical_scroll_pane_set_hscrollbar(LogicalScrollPane *this, GtkWidget *scrollbar)
{
	set_scrollbar(this, scrollbar, &this->hscrollbar, &this->hscrollbar_visible);
}

void
logical_scroll_pane_set_vscrollbar(LogicalScrollPane *this, GtkWidget *scrollbar)
{
	set_scrollbar(this, scrollbar, &this->vscrollbar, &this->vscrollbar_visible);
}

GtkWidget *
logical_scroll_pane_get_hscrollbar(LogicalScrollPane *this)
{
	return this->hscrollbar;
}

GtkWidget *
logical_scroll_pane_get_vscrollbar(LogicalScrollPane *this)
{
	return this->vscrollbar;
}

static void
set_scrollbar_visible(LogicalScrollPane *this, int visible, GtkWidget **scrollbar, int *scrollbar_visible)
{
	visible = visible != 0;

	if (*scrollbar_visible == visible)
		return;

	*scrollbar_visible = visible;

	if (*scrollbar)
		gtk_widget_set_visible(*scrollbar, visible);

	gtk_widget_queue_resize(GTK_WIDGET(this));
}

void
logical_scroll_pane_set_hscrollbar_visible(LogicalScrollPane *this, int visible)
{
	set_scrollbar_visible(this, visible, &this->hscrollbar, &this->hscrollbar_visible);
}

void
logical_scroll_pane_set_vscrollbar_visible(LogicalScrollPane *this, int visible)
{
	set_scrollbar_visible(this, visible, &this->vscrollbar, &this->vscrollbar_visible);
}

int
logical_scroll_pane_get_hscrollbar_visible(LogicalScrollPane *this)
{
	return this->hscrollbar_visible;
}

int
logical_scroll_pane_get_vscrollbar_visible(LogicalScrollPane *this)
{
	return this->vscrollbar_visible;
}

void
logical_scroll_pane_set_hadjustment(LogicalScrollPane *this, GtkAdjustment *adjustment)
{
	logical_scroll_pane_set_adjustment(this, 1, adjustment);
}

void
logical_scroll_pane_set_vadjustment(LogicalScrollPane *this, GtkAdjustment *adjustment)
{
	logical_scroll_pane_set_adjustment(this, 0, adjustment);
}

GtkAdjustment *
logical_scroll_pane_get_hadjustment(LogicalScrollPane *this)
{
	return this->hadjustment;
}

GtkAdjustment *
logical_scroll_pane_get_vadjustment(LogicalScrollPane *this)
{
	return this->vadjustment;
}
