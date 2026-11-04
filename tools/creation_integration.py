"""Exercise character creation/loading in a disposable copy of the original SQL contracts."""
import secrets
import re
import subprocess

from database import identifier, literal, sql


def run(command, build_directory="build"):
    name = "Eternity_Test_Creation_" + secrets.token_hex(6)
    catalog = identifier(name)
    collation = sql("SELECT CONVERT(varchar(128),DATABASEPROPERTYEX('PS_GameData','Collation'));")
    if not re.fullmatch(r"[A-Za-z0-9_]+", collation):
        raise ValueError("Invalid source database collation identifier")
    sql(f"CREATE DATABASE {catalog} COLLATE {collation};")
    try:
        tables = ("Chars", "CharItems", "UserMaxGrow", "GuildChars", "_DeletedChars", "UserStoredMoney", "CharEvents",
                  "UserStoredItems", "CharSkills", "CharApplySkills", "CharQuests", "CharQuickSlots", "CharSavePoint",
                  "PointLog", "ProductLog", "UserStoredPointItems", "MarketCharConcern", "FriendChars", "BanChars", "PointGiftNotify")
        for table in tables:
            sql(f"SELECT TOP (0) * INTO dbo.{identifier(table)} FROM PS_GameData.dbo.{identifier(table)};", name)
        sql("SELECT TOP (0) * INTO dbo.Users_Product FROM PS_Billing.dbo.Users_Product;", name)
        sql("SELECT TOP (0) * INTO dbo.Users_Master FROM PS_UserData.dbo.Users_Master;", name)
        for source_database, source_tables in (("PS_GameData", tables), ("PS_UserData", ("Users_Master",)), ("PS_Billing", ("Users_Product",))):
            names = ",".join(literal(table) for table in source_tables)
            defaults = sql("""SELECT t.name AS [table],c.name AS [column],d.definition
            FROM sys.default_constraints d JOIN sys.tables t ON t.object_id=d.parent_object_id
            JOIN sys.columns c ON c.object_id=t.object_id AND c.column_id=d.parent_column_id
            WHERE t.name IN (""" + names + ") FOR JSON PATH;", source_database, json_output=True)
            for default in defaults:
                sql(f"ALTER TABLE dbo.{identifier(default['table'])} ADD DEFAULT {default['definition']} "
                    f"FOR {identifier(default['column'])};", name)
        procedures = ("usp_Create_Char_R", "usp_Save_Char_Item_Add_E", "usp_Save_Char_Country_E", "usp_Read_Chars_R",
                      "usp_Read_Char_Items_Simple_R", "usp_Read_User_MaxGrow_R", "usp_Delete_Char_E", "usp_Read_Chars_Detail2_R",
                      "usp_Read_Char_Items_Detail_R", "usp_Read_User_StoredItems_Detail_R", "usp_Read_Char_Skills_R",
                      "usp_Read_Char_ApplySkills_R", "usp_Read_Char_Quest_R", "usp_Read_Char_FinishedQuest_R",
                      "usp_Read_Char_QuickSlots_R", "usp_Read_Char_SavePoint", "usp_Read_Chars_PointLog_R",
                      "usp_Read_User_StoredPointItems", "usp_Read_User_CashPoint_NCash", "usp_Read_Char_GuildJoinDate_R",
                      "usp_Read_Char_ConcernMarket_R", "usp_Read_FriendChar_R", "usp_Read_BanChar_R", "usp_Read_User_GiftPointItemNotify",
                      "usp_Save_Char_ApplySkill_Del_E", "usp_Save_Char_ApplySkill_Add_E2",
                      "usp_Save_Char_Item_Mod_E", "usp_Save_Char_Item_Del_E", "usp_Save_User_StoredItem_Add_E",
                      "usp_Save_User_StoredItem_Mod_E", "usp_Save_User_StoredItem_Del_E",
                      "usp_Save_Char_Info_E", "usp_Save_Char_BodyChg_E",
                      "usp_Save_Char_Skill_Add_E", "usp_Save_Char_Skill_Mod_E", "usp_Save_Char_Skill_Del_E",
                      "usp_Save_Char_Quest_Add_E", "usp_Save_Char_Quest_Mod_E", "usp_Save_Char_Quest_Del_E",
                      "usp_Save_Char_FinishedQuest_E", "usp_Save_Char_QuickSlot_Del_E", "usp_Save_Char_QuickSlot_Add_E",
                      "usp_Create_Char_SavePoint", "usp_Save_Char_SavePoint", "usp_Save_Char_ConcernMarket_Add_E",
                      "usp_Save_Char_ConcernMarket_Del_E", "usp_Save_Char_Maxgrow_E",
                      "usp_Read_Char_Product_Item_E", "usp_Save_Char_Product_Item_E", "usp_Save_User_StoredPointItems",
                      "usp_Save_User_BuyPointItems_NCash", "usp_Save_User_StoredMoney_E", "usp_Save_Char_LeaveDate_R")
        names = ",".join(literal(procedure) for procedure in procedures)
        modules = sql("""SELECT p.name,m.definition FROM sys.sql_modules m
            JOIN sys.procedures p ON p.object_id=m.object_id
            WHERE p.name IN (""" + names + ") FOR JSON PATH;", "PS_GameData", json_output=True)
        if len(modules) != len(procedures):
            raise ValueError("Original character procedure set is incomplete")
        for module in modules:
            definition = module["definition"]
            if module["name"].lower() == "usp_read_user_cashpoint_ncash":
                dependency = "PS_UserData.dbo.Users_Master"
                if dependency not in definition:
                    raise ValueError("Original cash procedure dependency changed")
                # Only the fixture redirects its cross-database account reference to the scratch account table.
                definition = definition.replace(dependency, "dbo.Users_Master")
            if module["name"].lower() in ("usp_read_char_product_item_e", "usp_save_char_product_item_e"):
                dependency = "PS_USERDB01.PS_Billing.dbo.Users_Product"
                if dependency not in definition:
                    raise ValueError("Original product procedure dependency changed")
                # Only this disposable fixture redirects the recovered linked-server table.
                # Keep BEGIN DISTRIBUTED TRANSACTION and all original save branches intact.
                definition = definition.replace(dependency, "dbo.Users_Product")
            if module["name"].lower() == "usp_save_user_buypointitems_ncash":
                dependency = "PS_UserData.dbo.usp_Update_UserPoint"
                if dependency not in definition:
                    raise ValueError("Original purchase point procedure dependency changed")
                definition = definition.replace(dependency, "dbo.usp_Update_UserPoint")
            sql(f"EXEC({literal(definition)});", name)
        point_update = sql("SELECT m.definition FROM sys.sql_modules m JOIN sys.procedures p "
                           "ON p.object_id=m.object_id WHERE p.name='usp_Update_UserPoint' FOR JSON PATH;",
                           "PS_UserData", json_output=True)
        if len(point_update) != 1:
            raise ValueError("Original account point update procedure is missing")
        sql(f"EXEC({literal(point_update[0]['definition'])});", name)
        # A failure on the second starter item proves the outer character transaction rolls back.
        sql("ALTER TABLE dbo.CharItems ADD CONSTRAINT EternityRejectFixtureItem CHECK(ItemID<>2147483000); "
            "ALTER TABLE dbo.CharApplySkills ADD CONSTRAINT EternityRejectFixtureEffect CHECK(SkillID<>32760); "
            "ALTER TABLE dbo.CharQuests ADD CONSTRAINT EternityRejectFixtureQuest CHECK(QuestID<>32760); "
            "ALTER TABLE dbo.CharSavePoint ADD CONSTRAINT EternityRejectFixturePoint CHECK(MapID<>32760); "
            "ALTER TABLE dbo.MarketCharConcern ADD CONSTRAINT EternityRejectFixtureMarket CHECK(MarketID<>2147483000); "
            "ALTER TABLE dbo.UserMaxGrow ADD CONSTRAINT EternityRejectFixtureGrow CHECK(UserUID<>9502 OR MaxGrow<>3); "
            "CREATE USER [eternity] FOR LOGIN [eternity]; "
            "GRANT SELECT,INSERT,UPDATE,DELETE,EXECUTE ON SCHEMA::dbo TO [eternity];", name)
        # Administrative fixture DDL remains outside the runtime account's grants.
        sql("CREATE SEQUENCE dbo.EternitySaveRetry AS int START WITH 1 INCREMENT BY 1 NO CACHE;", name)
        trigger = ("CREATE TRIGGER dbo.EternityFailFirstCore ON dbo.Chars AFTER UPDATE AS BEGIN SET NOCOUNT ON; "
            "IF UPDATE(PosX) AND EXISTS(SELECT 1 FROM inserted WHERE UserUID=9901) BEGIN "
            "DECLARE @Attempt int=NEXT VALUE FOR dbo.EternitySaveRetry; "
            "IF @Attempt=1 BEGIN THROW 50001,'Eternity first core save fixture failure',1; END; END; END;")
        sql(f"EXEC({literal(trigger)});", name)
        subprocess.run(command + [f"{build_directory}/creation-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/creation-network-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/players-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/vertical-entry-sql-integration", name], check=True)
        # Reset only this fresh disposable fixture after the preceding tests.
        # Duplicate UIDs then exercise the original failed-insert delete/retry path.
        sql("DELETE dbo.CharItems; DELETE dbo.UserStoredItems; "
            "CREATE UNIQUE INDEX EternityFixtureInventoryUID ON dbo.CharItems(ItemUID); "
            "CREATE UNIQUE INDEX EternityFixtureBankUID ON dbo.UserStoredItems(ItemUID);", name)
        subprocess.run(command + [f"{build_directory}/inventory-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/character-core-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/skill-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/quest-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/interface-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/account-state-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/point-items-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/purchases-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/character-recovery-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/character-save-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/vertical-persistence-sql-integration", name], check=True)
    finally:
        # Only the fresh, randomly named fixture catalog is removed.
        sql(f"ALTER DATABASE {catalog} SET SINGLE_USER WITH ROLLBACK IMMEDIATE; DROP DATABASE {catalog};")
