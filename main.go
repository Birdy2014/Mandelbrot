package main

import (
	"github.com/gotk3/gotk3/cairo"
	"github.com/gotk3/gotk3/gdk"
	"github.com/gotk3/gotk3/gtk"
	"math"
	"math/cmplx"
)

func fractal(c complex128, maxIter int) int {
	z := 0 + 0i
	i := 0
	for ; i < maxIter && cmplx.Abs(z) < 2; i++ {
		z = z * z + c
	}
	return i
}

func mandelbrot(width, height, maxIter int, x1, y1, x2, y2 float64, allowDistortion bool) []int {
	yratio = (y2 - y1) / float64(height)
	if allowDistortion {
		xratio = (x2 - x1) / float64(width)
	} else {
		xratio = yratio
	}
	set := make([]int, width * height)
	for i := range set {
		c := complex(x1 + xratio * float64(i%width), y1 + yratio * float64(i/width))
		set[i] = fractal(c, maxIter)
	}
	return set
}

func drawMandelbrot(da *gtk.DrawingArea, cr *cairo.Context) {
	var set []int
	if drawLine || (x1old == x1 && y1old == y1 && x2old == x2 && y2old == y2 && oldwidth == width && oldHeight == height && xratio == xratioold && yratio == yratioold) {
		set = oldSet
	} else {
		set = mandelbrot(width, height, 300, x1, y1, x2, y2, allowDistortion)
		oldSet = set
		x1old, y1old, x2old, y2old, xratioold, yratioold = x1, y1, x2, y2, xratio, yratio
	}

	//TODO use faster rendering
	for i, c := range set {
		if c == 300 {
			cr.SetSourceRGB(0, 0, 0)
		} else if c > 200 {
			cr.SetSourceRGB(float64(c) / 400, float64(c) / 600, float64(c) / 500)
		} else if c > 100 {
			cr.SetSourceRGB(float64(c) / 200, float64(c) / 200, float64(c) / 300)
		} else {
			cr.SetSourceRGB(float64(c) / 60, float64(c) / 40, float64(c) / 40)
		}

		cr.Rectangle(float64(i%width), float64(i/width), 1, 1)
		cr.Fill()
	}

	if drawLine && secondClick {
		cr.SetSourceRGB(1, 0, 0)
		cr.Arc(x1pix, y1pix, 2, 0, 2 * math.Pi)
		cr.Fill()
	} else if drawLine {
		cr.SetSourceRGB(1, 0, 0)
		cr.MoveTo(x1pix, y1pix)
		cr.LineTo(x2pix, y1pix)
		cr.LineTo(x2pix, y2pix)
		cr.LineTo(x1pix, y2pix)
		cr.LineTo(x1pix, y1pix)
		cr.Stroke()
		drawLine = false
	}
}

var width, height int											//Size of the drawingarea, updated when resized
var x1, y1, x2, y2 float64 = -2, -1.25, 0.5, 1.25   			//mandelbrot coordinates to render on next draw
var x1old, y1old, x2old, y2old float64							//old coordinates and size so it doesn't have to re-render everything if nothing changes
var oldwidth, oldHeight int
var xratio, yratio, xratioold, yratioold, x1new, y1new float64            			//ratio pixel - coordinates and the new top left coordinates that are set on the first click
var secondClick, drawLine bool
var x1pix, y1pix, x2pix, y2pix float64							//clicked position in pixels on screen for drawing lines
var oldSet []int
var allowDistortion bool = false
func main() {
	gtk.Init(nil)
	win, _ := gtk.WindowNew(gtk.WINDOW_TOPLEVEL)

	win.SetTitle("Mandelbrot")
	win.Connect("destroy", func() {
		gtk.MainQuit()
	})

	box, _ := gtk.BoxNew(gtk.ORIENTATION_VERTICAL, 1)
	buttonbox, _ := gtk.BoxNew(gtk.ORIENTATION_HORIZONTAL, 1)

	b, _ := gtk.ButtonNewWithLabel("Generate Mandelbrot")
	b.Connect("clicked", win.QueueDraw)

	buttonReset, _ := gtk.ButtonNewWithLabel("Reset")
	buttonReset.Connect("clicked", func() {
		x1, y1, x2, y2 = -2, -1.25, 0.5, 1.25
		win.QueueDraw()
	})

	buttonResetAspect, _ := gtk.ButtonNewWithLabel("Reset aspect ratio")
	buttonResetAspect.Connect("clicked", func() {
		yratio = xratio
		win.QueueDraw()
	})

	da, _ := gtk.DrawingAreaNew()
	da.AddEvents(gdk.GDK_BUTTON1_MASK)
	da.Connect("size-allocate", func(da *gtk.DrawingArea) {
		oldwidth = width
		oldHeight = height
		allocation := da.GetAllocation()
		width = allocation.GetWidth()
		height = allocation.GetHeight()
	})
	da.Connect("button-press-event", func(da *gtk.DrawingArea, event *gdk.Event) {
		secondClick = !secondClick
		eventButton := gdk.EventButtonNewFromEvent(event)
		if secondClick {
			x1new = x1 + xratio * eventButton.X()
			y1new = y1 + yratio * eventButton.Y()

			x1pix = eventButton.X()
			y1pix = eventButton.Y()

			drawLine = true
			win.QueueDraw()
		} else {
			x2 = x1 + xratio * eventButton.X()
			y2 = y1 + yratio * eventButton.Y()
			x1 = x1new
			y1 = y1new

			x2pix = eventButton.X()
			y2pix = eventButton.Y()

			drawLine = true
			win.QueueDraw()
		}
	})
	da.Connect("draw", drawMandelbrot)

	box.PackStart(da, true, true, 0)
	buttonbox.Add(b)
	buttonbox.Add(buttonReset)
	buttonbox.Add(buttonResetAspect)
	box.Add(buttonbox)
	win.Add(box)

	win.SetDefaultSize(800, 600)

	win.ShowAll()

	gtk.Main()
}