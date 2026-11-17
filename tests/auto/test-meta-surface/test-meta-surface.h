#include "qcustomplot.h"
#include <QtTest/QtTest>
#include <functional>

// The meta-object surface of milestone 5e: every property with a NOTIFY fires it once per change,
// after the plot's delivery, and binds through QBindable(obj, "name") without a storage member.
class TestMetaSurface : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void spikeSignalIsDeferredAndFiresOnce_data();
    void spikeSignalIsDeferredAndFiresOnce();
    void spikeSubscriberSeesTheChangeAfterDelivery_data();
    void spikeSubscriberSeesTheChangeAfterDelivery();
    void spikeBindingDrivesTheSetterAndRepaints_data();
    void spikeBindingDrivesTheSetterAndRepaints();
    void spikeSlotDeletingTheObject_data();
    void spikeSlotDeletingTheObject();
    void spikeObserverDeletingTheObject_data();
    void spikeObserverDeletingTheObject();
    void spikeSlotDeletingThePlot_data();
    void spikeSlotDeletingThePlot();
    void spikeObserverDeletingThePlot_data();
    void spikeObserverDeletingThePlot();
    void spikeBindingWhoseSourceDiesFirst_data();
    void spikeBindingWhoseSourceDiesFirst();
    void bindingToARefusedScaleTypeKeepsTheValue();
    void plainWriteDoesNotBreakTheBinding();
    void curvePenNotifiesOnce();
    void componentRepenRepaintsWithoutPenChanged();

    void everyPropertyIsConstantOrNotifies_data();
    void everyPropertyIsConstantOrNotifies();
    void everyWriterBesidesTheSetterNotifies_data();
    void everyWriterBesidesTheSetterNotifies();
    void nestedWriterSignalsWaitForTheWriter_data();
    void nestedWriterSignalsWaitForTheWriter();
    void layoutPassSignalsAfterTheReplot();
    void themeSlotDeletingThePlot_data();
    void themeSlotDeletingThePlot();
    void slotDeletingThePlotDuringAWriter_data();
    void slotDeletingThePlotDuringAWriter();
    void exceptionsNameRealProperties();
    void everyFormRowHolds_data();
    void everyFormRowHolds();
    void everyGadgetPropertyReadsWhatItWrites_data();
    void everyGadgetPropertyReadsWhatItWrites();
    void revisionNotifiesThroughChanged_data();
    void revisionNotifiesThroughChanged();
    void gadgetSettersKeepTheirRules();
    void spanSlotMovingAnEdgeIsHeard();
    void deferredPointerToADeadObjectArrivesNull();
    void antialiasingSetsStayApart();
    void polarNumberFormatIsWhatIsDrawn_data();
    void polarNumberFormatIsWhatIsDrawn();
    void everyNotifyPropertyNotifiesOncePerChange_data();
    void everyNotifyPropertyNotifiesOncePerChange();
    void exceptionRulesHold_data();
    void exceptionRulesHold();

    void everyEnumPropertyNamesARegisteredEnum_data();
    void everyEnumPropertyNamesARegisteredEnum();
    void publicEnumsAreRegistered_data();
    void publicEnumsAreRegistered();
    void invokablesAreFound_data();
    void invokablesAreFound();
    void metatypesKeepTheirNames_data();
    void metatypesKeepTheirNames();
    void invokableOnABaseIsFoundThroughADerivedClass();
};

// Runs its action when a signal it is connected to through QMetaMethod fires.
class MetaTrigger : public QObject
{
    Q_OBJECT
public:
    std::function<void()> action;
public slots:
    void fire()
    {
        if (action)
            action();
    }
};
