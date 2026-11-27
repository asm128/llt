#include "llc_engine.h"
#include "llc_window.h"
#include "llc_gui_inputbox.h"
#include "llc_gui_viewport.h"
#include "llc_minmax.h"

#ifndef LLC_SAMPLE_GUI_COMPONENTS_H_23627
#define LLC_SAMPLE_GUI_COMPONENTS_H_23627

stxp ::llc::u0_t SAMPLE_GUI_COMPONENTS_COUNT = 3;

stct SSampleCheckBox {
	::llc::cid_t		IdControl	= ::llc::CID_INVALID;
	bool			Checked		= false;
};

stct SSampleSlider {
	::llc::cid_t		IdRoot		= ::llc::CID_INVALID;
	::llc::cid_t		IdThumb		= ::llc::CID_INVALID;
	::llc::minmaxs3_t	ValueLimits	= {0, 100};
	::llc::s3_t		ValueCurrent	= 0;
	::llc::s3_t		ValueStep	= 1;
	::llc::s3_t		ValuePage	= 10;
	bool			Vertical	= false;
	::llc::astsc_t<64>	ValueText	= {};
};

stct SSampleGUIComponents {
	::llc::SEngine		Engine			= {};
	::llc::SWindow		& Window;
	::llc::SGUI			& GUI;
	::llc::cid_t			Desktop			= ::llc::CID_INVALID;
	::llc::cid_t			Help			= ::llc::CID_INVALID;
	::llc::cid_t			Performance		= ::llc::CID_INVALID;
	::llc::astsc_t<96>		PerformanceText	= {};
	::llc::cid_t			ComponentsPanel	= ::llc::CID_INVALID;
	::llc::cid_t			ControlEdit		= ::llc::CID_INVALID;
	::llc::cid_t			ControlExit		= ::llc::CID_INVALID;
	SSampleCheckBox		CheckBox			= {};
	SSampleSlider		SliderHorizontal	= {};
	SSampleSlider		SliderVertical		= {};
	::llc::SInputBox		InputBox			= {};
	::llc::n2s1_t			InputBoxDragOrigin	= {};
	bool					InputBoxDragging	= false;
	bool					InputBoxDragCanceled	= false;
	::llc::acid			ControlsKeyboard	= {};
	::llc::astatic<::llc::SViewport, SAMPLE_GUI_COMPONENTS_COUNT>
						Viewports		= {};
	::llc::astatic<::llc::n2s1_t, SAMPLE_GUI_COMPONENTS_COUNT>
						UnfoldedSize		= {};
	::llc::astatic<bool, SAMPLE_GUI_COMPONENTS_COUNT>
						Folded			= {};

	SSampleGUIComponents(::llc::SWindow & window, ::llc::SGUI & gui) : Window(window), GUI(gui) {}
};

::llc::err_t	sampleGUIComponentsSetup				(SSampleGUIComponents & app);
::llc::err_t	sampleGUIComponentsUpdate				(SSampleGUIComponents & app, bool systemRequestedExit, double secondsLastFrame);
::llc::err_t	sampleGUIComponentsDraw					(SSampleGUIComponents & app, ::llc::img8bgra (&clientImages)[SAMPLE_GUI_COMPONENTS_COUNT], ::llc::u2_t frameIndex, ::llc::g8bgra target);

#endif // LLC_SAMPLE_GUI_COMPONENTS_H_23627
