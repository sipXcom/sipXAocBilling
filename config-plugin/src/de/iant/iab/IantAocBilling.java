/*
 *
 * Copyright (C) 2012 eZuce Inc.
 *
 * $
 */
package de.iant.iab;

import org.sipfoundry.sipxconfig.address.AddressType;
import org.sipfoundry.sipxconfig.feature.LocationFeature;

public interface IantAocBilling {
    public static final LocationFeature IAB_FEATURE = new LocationFeature("iantab");
    IantAocBillingSettings getSettings();

    void saveSettings(IantAocBillingSettings settings);

}
